#include "appSplitTunnelingUiController.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

#include "core/utils/errorCodes.h"
#include "core/utils/folderExecutables.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

namespace
{
    constexpr const int largeFolderExecutablesCount = 100;
}

AppSplitTunnelingUiController::AppSplitTunnelingUiController(AppSplitTunnelingController* appSplitTunnelingController,
                                                              AppSplitTunnelingModel* appSplitTunnelingModel,
                                                              QObject *parent)
    : QObject(parent),
      m_appSplitTunnelingController(appSplitTunnelingController),
      m_appSplitTunnelingModel(appSplitTunnelingModel)
{
    updateModel();
}

void AppSplitTunnelingUiController::addApp(const QString &appPath)
{
    amnezia::InstalledAppInfo appInfo { "", "", appPath };
    if (!appPath.isEmpty()) {
        QFileInfo fileInfo(appPath);
        appInfo.appName = fileInfo.fileName();
    }

    if (m_appSplitTunnelingController->addApp(appInfo)) {
        emit finished(tr("Application added: %1").arg(appInfo.appName));
    } else {
        emit errorOccurred(tr("The application has already been added"));
    }
}

void AppSplitTunnelingUiController::addApps(QVector<QPair<QString, QString>> apps)
{
    for (const auto &app : apps) {
        amnezia::InstalledAppInfo appInfo { app.first, app.second, "" };
        m_appSplitTunnelingController->addApp(appInfo);
    }
    emit finished(tr("The selected applications have been added"));
}

void AppSplitTunnelingUiController::addFolder(const QUrl &folderUrl)
{
    const QString folderPath = folderUrl.toLocalFile();
    if (!folderExecutables::hasDriveLetter(folderPath)) {
        emit errorOccurred(tr("Network folders are not supported, choose a folder on a local drive"));
        return;
    }
    if (!m_appSplitTunnelingController->addFolder(folderPath)) {
        emit errorOccurred(tr("The folder has already been added"));
        return;
    }

    auto watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, folderPath]() {
        const int executablesCount = watcher->result().size();
        watcher->deleteLater();

        const QString nativePath = QDir::toNativeSeparators(folderPath);
        if (executablesCount >= folderExecutables::maxExecutables) {
            emit finished(tr("Folder added: %1. It has too many executable files, only the first %2 found will bypass "
                             "the VPN")
                                  .arg(nativePath)
                                  .arg(folderExecutables::maxExecutables));
        } else if (executablesCount >= largeFolderExecutablesCount) {
            emit finished(tr("Folder added: %1. It contains %2 executable files, all of them will bypass the VPN")
                                  .arg(nativePath)
                                  .arg(executablesCount));
        } else {
            emit finished(tr("Folder added: %1. Executable files found: %2").arg(nativePath).arg(executablesCount));
        }
    });
    watcher->setFuture(QtConcurrent::run(&folderExecutables::find, folderPath));
}

void AppSplitTunnelingUiController::removeApp(const int index)
{
    auto modelIndex = m_appSplitTunnelingModel->index(index);
    const int appsCount = m_appSplitTunnelingController->getApps().size();
    if (index >= appsCount) {
        auto folderPath =
                m_appSplitTunnelingModel->data(modelIndex, AppSplitTunnelingModel::Roles::FullPathRole).toString();
        m_appSplitTunnelingController->removeFolder(index - appsCount);
        emit finished(tr("Folder removed: %1").arg(folderPath));
        return;
    }

    auto appPath = m_appSplitTunnelingModel->data(modelIndex, AppSplitTunnelingModel::Roles::AppPathRole).toString();
    m_appSplitTunnelingController->removeApp(index);

    QFileInfo fileInfo(appPath);
    emit finished(tr("Application removed: %1").arg(fileInfo.fileName()));
}

void AppSplitTunnelingUiController::toggleSplitTunneling(bool enabled)
{
    m_appSplitTunnelingController->toggleSplitTunneling(enabled);
    emit isSplitTunnelingEnabledChanged();
}

void AppSplitTunnelingUiController::setRouteMode(int routeMode)
{
    m_appSplitTunnelingController->setRouteMode(static_cast<amnezia::AppsRouteMode>(routeMode));
    emit routeModeChanged();
}

int AppSplitTunnelingUiController::getRouteMode() const
{
    return static_cast<int>(m_appSplitTunnelingController->getRouteMode());
}

bool AppSplitTunnelingUiController::isSplitTunnelingEnabled() const
{
    return m_appSplitTunnelingController->isSplitTunnelingEnabled();
}

void AppSplitTunnelingUiController::updateModel()
{
    m_appSplitTunnelingModel->updateModel(m_appSplitTunnelingController->getApps(),
                                          m_appSplitTunnelingController->getFolders());
}

