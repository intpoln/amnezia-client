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

void AppSplitTunnelingUiController::addAppFiles(const QList<QUrl> &fileUrls)
{
    QStringList appPaths;
    for (const auto &fileUrl : fileUrls) {
        appPaths.append(fileUrl.toLocalFile());
    }
    addAppPaths(appPaths);
}

void AppSplitTunnelingUiController::addAppPaths(const QStringList &appPaths)
{
    if (appPaths.isEmpty()) {
        return;
    }

    if (appPaths.size() == 1) {
        addApp(appPaths.first());
        return;
    }

    int addedCount = 0;
    for (const auto &appPath : appPaths) {
        if (m_appSplitTunnelingController->addApp({ QFileInfo(appPath).fileName(), "", appPath })) {
            ++addedCount;
        }
    }

    if (addedCount == appPaths.size()) {
        emit finished(tr("Applications added: %1").arg(addedCount));
    } else {
        emit finished(
                tr("Applications added: %1, already in the list: %2").arg(addedCount).arg(appPaths.size() - addedCount));
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
    addFolderPath(folderUrl.toLocalFile());
}

void AppSplitTunnelingUiController::addAppFolders(const QStringList &appPaths)
{
    QStringList folderPaths;
    for (const auto &appPath : appPaths) {
        const QString folderPath = QFileInfo(appPath).absolutePath();
        if (!folderPaths.contains(folderPath, Qt::CaseInsensitive)) {
            folderPaths.append(folderPath);
        }
    }

    for (const auto &folderPath : std::as_const(folderPaths)) {
        addFolderPath(folderPath);
    }
}

void AppSplitTunnelingUiController::scanFolders()
{
    for (const auto &folderPath : m_appSplitTunnelingController->getFolders()) {
        if (m_appSplitTunnelingModel->folderExecutablesCount(folderPath)
            == AppSplitTunnelingModel::executablesCountUnknown) {
            scanFolder(folderPath, false);
        }
    }
}

void AppSplitTunnelingUiController::addFolderPath(const QString &folderPath)
{
    if (!folderExecutables::hasDriveLetter(folderPath)) {
        emit errorOccurred(tr("Network folders are not supported, choose a folder on a local drive"));
        return;
    }
    if (!m_appSplitTunnelingController->addFolder(folderPath)) {
        emit errorOccurred(tr("The folder has already been added"));
        return;
    }

    scanFolder(QDir::cleanPath(folderPath), true);
}

void AppSplitTunnelingUiController::scanFolder(const QString &folderPath, bool reportResult)
{
    m_appSplitTunnelingModel->setFolderExecutablesCount(folderPath, AppSplitTunnelingModel::executablesCountSearching);

    auto watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, folderPath, reportResult]() {
        const int executablesCount = watcher->result().size();
        watcher->deleteLater();

        m_appSplitTunnelingModel->setFolderExecutablesCount(folderPath, executablesCount);
        if (!reportResult) {
            return;
        }

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

int AppSplitTunnelingUiController::appsCount() const
{
    return m_appSplitTunnelingController->getApps().size();
}

int AppSplitTunnelingUiController::foldersCount() const
{
    return m_appSplitTunnelingController->getFolders().size();
}

int AppSplitTunnelingUiController::maxFolderExecutables() const
{
    return folderExecutables::maxExecutables;
}

void AppSplitTunnelingUiController::updateModel()
{
    m_appSplitTunnelingModel->updateModel(m_appSplitTunnelingController->getApps(),
                                          m_appSplitTunnelingController->getFolders());
    emit exclusionsChanged();
}

