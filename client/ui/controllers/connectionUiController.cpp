#include "connectionUiController.h"

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(MACOS_NE)
    #include <QGuiApplication>
#else
    #include <QApplication>
#endif

#include "amneziaApplication.h"
#include "core/controllers/serversController.h"
#include "core/models/containerConfig.h"
#include "core/utils/containerEnum.h"

ConnectionUiController::ConnectionUiController(ConnectionController* connectionController,
                                                ServersController* serversController,
                                                QObject *parent)
    : QObject(parent),
      m_connectionController(connectionController),
      m_serversController(serversController)
{
    connect(m_connectionController, &ConnectionController::connectionStateChanged, this, &ConnectionUiController::onConnectionStateChanged);
    connect(m_connectionController, &ConnectionController::splitTunnelFailed, this,
            &ConnectionUiController::onSplitTunnelFailed);

    connect(this, &ConnectionUiController::connectButtonClicked, this, &ConnectionUiController::toggleConnection, Qt::QueuedConnection);

    m_state = Vpn::ConnectionState::Disconnected;
}

void ConnectionUiController::openConnection()
{
    const QString serverId = m_serversController->getDefaultServerId();
    if (serverId.isEmpty()) {
        m_connectionController->setConnectionState(Vpn::ConnectionState::Disconnected);
        return;
    }

    ErrorCode errorCode = m_connectionController->openConnection(serverId);

    if (errorCode != ErrorCode::NoError) {
        notifyConnectionBlocked(errorCode);
        return;
    }
}

void ConnectionUiController::closeConnection()
{
    m_isReconnectRequested = false;
    m_reconnectPurpose = ReconnectPurpose::None;
    m_connectionController->closeConnection();
}

void ConnectionUiController::reconnect()
{
    requestReconnect(ReconnectPurpose::Reconnect);
}

void ConnectionUiController::switchConnection()
{
    requestReconnect(ReconnectPurpose::Switch);
}

void ConnectionUiController::requestReconnect(ReconnectPurpose purpose)
{
    if (m_state == Vpn::ConnectionState::Preparing) {
        emit preparingConfig();
        return;
    }

    if (!isConnected() && !isConnectionInProgress()) {
        toggleConnection();
        return;
    }

    closeConnection();
    m_isReconnectRequested = true;
    m_reconnectPurpose = purpose;

    const QString stateText = reconnectStateText();
    if (!stateText.isEmpty()) {
        m_connectionStateText = stateText;
        emit connectionStateChanged();
    }
}

QString ConnectionUiController::reconnectStateText() const
{
    if (m_state != Vpn::ConnectionState::Disconnecting && m_state != Vpn::ConnectionState::Preparing
        && m_state != Vpn::ConnectionState::Connecting) {
        return {};
    }

    switch (m_reconnectPurpose) {
    case ReconnectPurpose::Reconnect: return tr("Reconnecting...");
    case ReconnectPurpose::Switch: return tr("Switching...");
    case ReconnectPurpose::None: break;
    }
    return {};
}

void ConnectionUiController::retryConnection()
{
    if (m_state == Vpn::ConnectionState::Preparing || isConnected() || isConnectionInProgress()) {
        return;
    }

    startConnection();
}

void ConnectionUiController::continueReconnect()
{
    if (!m_isReconnectRequested) {
        return;
    }

    m_isReconnectRequested = false;
    retryConnection();
}

bool ConnectionUiController::isReconnectNeeded() const
{
    return m_isReconnectNeeded;
}

void ConnectionUiController::onAppExclusionsChanged()
{
    if (isConnected() || isConnectionInProgress()) {
        setReconnectNeeded(true);
    }
}

void ConnectionUiController::setReconnectNeeded(bool isReconnectNeeded)
{
    if (m_isReconnectNeeded != isReconnectNeeded) {
        m_isReconnectNeeded = isReconnectNeeded;
        emit isReconnectNeededChanged();
    }
}

bool ConnectionUiController::isSplitTunnelFailed() const
{
    return m_isSplitTunnelFailed;
}

void ConnectionUiController::setSplitTunnelFailed(bool isSplitTunnelFailed)
{
    if (m_isSplitTunnelFailed != isSplitTunnelFailed) {
        m_isSplitTunnelFailed = isSplitTunnelFailed;
        emit isSplitTunnelFailedChanged();
    }
}

void ConnectionUiController::onSplitTunnelFailed()
{
    setSplitTunnelFailed(true);
    if (m_state == Vpn::ConnectionState::Connected) {
        emit splitTunnelFailed();
    } else {
        m_isSplitTunnelFailureNotificationPending = true;
    }
}

ErrorCode ConnectionUiController::getLastConnectionError()
{
    return m_connectionController->lastConnectionError();
}

void ConnectionUiController::onConnectionStateChanged(Vpn::ConnectionState state)
{
    m_state = state;

    if (state == Vpn::ConnectionState::Disconnected || state == Vpn::ConnectionState::Error
        || state == Vpn::ConnectionState::Reconnecting) {
        setReconnectNeeded(false);
    }

    if (state == Vpn::ConnectionState::Connected) {
        if (m_isSplitTunnelFailureNotificationPending) {
            m_isSplitTunnelFailureNotificationPending = false;
            emit splitTunnelFailed();
        }
    } else if (state != Vpn::ConnectionState::Connecting) {
        m_isSplitTunnelFailureNotificationPending = false;
        setSplitTunnelFailed(false);
    }

    if (m_isReconnectRequested && (state == Vpn::ConnectionState::Disconnected || state == Vpn::ConnectionState::Error)) {
        if (state == Vpn::ConnectionState::Disconnected) {
            QMetaObject::invokeMethod(this, &ConnectionUiController::continueReconnect, Qt::QueuedConnection);
        } else {
            m_isReconnectRequested = false;
        }
    }

    if (state == Vpn::ConnectionState::Connected || state == Vpn::ConnectionState::Error
        || state == Vpn::ConnectionState::Unknown
        || (state == Vpn::ConnectionState::Disconnected && !m_isReconnectRequested)) {
        m_reconnectPurpose = ReconnectPurpose::None;
    }

    m_isConnected = false;
    m_connectionStateText = tr("Connecting...");
    switch (state) {
    case Vpn::ConnectionState::Connected: {
        amnApp->networkManager()->clearConnectionCache();

        m_isConnectionInProgress = false;
        m_isConnected = true;
        m_connectionStateText = tr("Connected");
        break;
    }
    case Vpn::ConnectionState::Connecting: {
        m_isConnectionInProgress = true;
        break;
    }
    case Vpn::ConnectionState::Reconnecting: {
        m_isConnectionInProgress = true;
        m_connectionStateText = tr("Reconnecting...");
        break;
    }
    case Vpn::ConnectionState::Disconnected: {
        m_isConnectionInProgress = false;
        m_connectionStateText = tr("Connect");
        break;
    }
    case Vpn::ConnectionState::Disconnecting: {
        m_isConnectionInProgress = true;
        m_connectionStateText = tr("Disconnecting...");
        break;
    }
    case Vpn::ConnectionState::Preparing: {
        m_isConnectionInProgress = true;
        m_connectionStateText = tr("Preparing...");
        break;
    }
    case Vpn::ConnectionState::Error: {
        m_isConnectionInProgress = false;
        m_connectionStateText = tr("Connect");
        emit connectionErrorOccurred(getLastConnectionError());
        break;
    }
    case Vpn::ConnectionState::Unknown: {
        m_isConnectionInProgress = false;
        m_connectionStateText = tr("Connect");
        emit connectionErrorOccurred(getLastConnectionError());
        break;
    }
    }

    const QString reconnectText = reconnectStateText();
    if (!reconnectText.isEmpty()) {
        m_connectionStateText = reconnectText;
    }
    emit connectionStateChanged();
}

void ConnectionUiController::onTranslationsUpdated()
{
    onConnectionStateChanged(getCurrentConnectionState());
}

Vpn::ConnectionState ConnectionUiController::getCurrentConnectionState()
{
    return m_state;
}

QString ConnectionUiController::connectionStateText() const
{
    return m_connectionStateText;
}

void ConnectionUiController::toggleConnection()
{
    if (m_state == Vpn::ConnectionState::Preparing) {
        emit preparingConfig();
        return;
    }

    if (isConnectionInProgress()) {
        closeConnection();
    } else if (isConnected()) {
        closeConnection();
    } else {
        startConnection();
    }
}

void ConnectionUiController::startConnection()
{
    const QString serverId = m_serversController->getDefaultServerId();
    if (serverId.isEmpty()) {
        return;
    }

    const ErrorCode errorCode = m_connectionController->isConnectionSupported(serverId);
    if (errorCode != ErrorCode::NoError) {
        notifyConnectionBlocked(errorCode);
        return;
    }

    emit prepareConfig();
}

void ConnectionUiController::notifyConnectionBlocked(ErrorCode errorCode)
{
    m_reconnectPurpose = ReconnectPurpose::None;

    if (errorCode == ErrorCode::LegacyApiV1NotSupportedError) {
        emit unsupportedConnectDrawerRequested();
        return;
    }

    if (errorCode == ErrorCode::NoInstalledContainersError) {
        emit noInstalledContainers();
        return;
    }

    emit connectionErrorOccurred(errorCode);
}

bool ConnectionUiController::isConnectionInProgress() const
{
    return m_isConnectionInProgress;
}

bool ConnectionUiController::isConnected() const
{
    return m_isConnected;
}

bool ConnectionUiController::isRevokeBlockedDuringActiveConnection(const QString &serverId, int containerIndex,
                                                                   const QString &clientId) const
{
    if (clientId.isEmpty() || (!isConnected() && !isConnectionInProgress())) {
        return false;
    }

    if (m_serversController->getDefaultServerId() != serverId) {
        return false;
    }

    if (static_cast<int>(m_serversController->getDefaultContainer(serverId)) != containerIndex) {
        return false;
    }

    const auto adminConfig = m_serversController->selfHostedAdminConfig(serverId);
    if (!adminConfig.has_value()) {
        return false;
    }

    const QString connectionClientId =
            adminConfig->containerConfig(static_cast<DockerContainer>(containerIndex)).protocolConfig.clientId();
    if (connectionClientId.isEmpty()) {
        return false;
    }

    return connectionClientId == clientId || connectionClientId.contains(clientId);
}
