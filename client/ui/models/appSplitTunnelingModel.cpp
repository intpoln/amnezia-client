#include "appSplitTunnelingModel.h"

#include <QDir>
#include <QFileInfo>

AppSplitTunnelingModel::AppSplitTunnelingModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AppSplitTunnelingModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_apps.size() + m_folders.size();
}

QVariant AppSplitTunnelingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rowCount()))
        return QVariant();

    const bool isFolder = index.row() >= m_apps.size();
    const QString folder = isFolder ? m_folders.at(index.row() - m_apps.size()) : QString();

    switch (role) {
        case AppPathRole: {
            if (isFolder) {
                const QString folderName = QFileInfo(folder).fileName();
                return folderName.isEmpty() ? QDir::toNativeSeparators(folder) : folderName;
            }
            return m_apps.at(index.row()).appName;
        }
        case FullPathRole: {
            return QDir::toNativeSeparators(isFolder ? folder : m_apps.at(index.row()).appPath);
        }
        case IsFolderRole: {
            return isFolder;
        }
        case ExecutablesCountRole: {
            return isFolder ? folderExecutablesCount(folder) : executablesCountUnknown;
        }
        default: {
            return true;
        }
    }

    return QVariant();
}

void AppSplitTunnelingModel::updateModel(const QVector<amnezia::InstalledAppInfo> &apps, const QStringList &folders)
{
    beginResetModel();
    m_apps = apps;
    m_folders = folders;
    endResetModel();
}

void AppSplitTunnelingModel::setFolderExecutablesCount(const QString &folder, int count)
{
    m_folderExecutablesCounts.insert(folder, count);

    const int folderIndex = m_folders.indexOf(folder);
    if (folderIndex >= 0) {
        const QModelIndex modelIndex = index(m_apps.size() + folderIndex);
        emit dataChanged(modelIndex, modelIndex, { ExecutablesCountRole });
    }
}

int AppSplitTunnelingModel::folderExecutablesCount(const QString &folder) const
{
    return m_folderExecutablesCounts.value(folder, executablesCountUnknown);
}

QHash<int, QByteArray> AppSplitTunnelingModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AppPathRole] = "appPath";
    roles[FullPathRole] = "fullPath";
    roles[IsFolderRole] = "isFolder";
    roles[ExecutablesCountRole] = "executablesCount";
    return roles;
}
