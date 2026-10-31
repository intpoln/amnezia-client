#ifndef APPSPLITTUNNELINGMODEL_H
#define APPSPLITTUNNELINGMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QVector>

#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

class AppSplitTunnelingModel: public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        AppPathRole = Qt::UserRole + 1,
        PackageAppNameRole,
        PackageAppIconRole,
        FullPathRole,
        IsFolderRole,
        ExecutablesCountRole
    };

    static constexpr int executablesCountUnknown = -2;
    static constexpr int executablesCountSearching = -1;

    explicit AppSplitTunnelingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void updateModel(const QVector<amnezia::InstalledAppInfo> &apps, const QStringList &folders);
    void setFolderExecutablesCount(const QString &folder, int count);
    int folderExecutablesCount(const QString &folder) const;

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<amnezia::InstalledAppInfo> m_apps;
    QStringList m_folders;
    QHash<QString, int> m_folderExecutablesCounts;
};

#endif // APPSPLITTUNNELINGMODEL_H
