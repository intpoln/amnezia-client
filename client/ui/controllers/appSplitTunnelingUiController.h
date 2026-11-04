#ifndef APPSPLITTUNNELINGUICONTROLLER_H
#define APPSPLITTUNNELINGUICONTROLLER_H

#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVector>

#include "core/controllers/appSplitTunnelingController.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"
#include "ui/models/appSplitTunnelingModel.h"

class AppSplitTunnelingUiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int routeMode READ getRouteMode WRITE setRouteMode NOTIFY routeModeChanged)
    Q_PROPERTY(bool isSplitTunnelingEnabled READ isSplitTunnelingEnabled NOTIFY isSplitTunnelingEnabledChanged)
    Q_PROPERTY(int appsCount READ appsCount NOTIFY exclusionsChanged)
    Q_PROPERTY(int foldersCount READ foldersCount NOTIFY exclusionsChanged)
    Q_PROPERTY(int maxFolderExecutables READ maxFolderExecutables CONSTANT)

public:
    explicit AppSplitTunnelingUiController(AppSplitTunnelingController* appSplitTunnelingController,
                                          AppSplitTunnelingModel* appSplitTunnelingModel,
                                          QObject *parent = nullptr);

public slots:
    void addApp(const QString &appPath);
    void addAppFiles(const QList<QUrl> &fileUrls);
    void addAppPaths(const QStringList &appPaths);
    void addApps(QVector<QPair<QString, QString>> apps);
    void addFolder(const QUrl &folderUrl);
    void addAppFolders(const QStringList &appPaths);
    QStringList appFolders(const QStringList &appPaths) const;
    QStringList folderExecutables(const QString &folderPath) const;
    bool isFolderScanning(const QString &folderPath) const;
    void scanFolders();
    void removeApp(const int index);
    void toggleSplitTunneling(bool enabled);
    void setRouteMode(int routeMode);

    int getRouteMode() const;
    bool isSplitTunnelingEnabled() const;
    int appsCount() const;
    int foldersCount() const;
    int maxFolderExecutables() const;

    void updateModel();

signals:
    void routeModeChanged();
    void isSplitTunnelingEnabledChanged();
    void exclusionsChanged();
    void folderScanned();
    void errorOccurred(const QString &errorMessage);
    void finished(const QString &message);

private:
    void addFolderPath(const QString &folderPath);
    void scanFolder(const QString &folderPath, bool reportResult);

    AppSplitTunnelingController* m_appSplitTunnelingController;
    AppSplitTunnelingModel* m_appSplitTunnelingModel;
    QSet<QString> m_scanningFolders;
    QHash<QString, QStringList> m_folderExecutables;
};

#endif // APPSPLITTUNNELINGUICONTROLLER_H
