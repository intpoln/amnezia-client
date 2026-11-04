#include "runningAppsModel.h"

#include <QDir>
#include <QFileInfo>

#include "core/utils/runningApps.h"

RunningAppsModel::RunningAppsModel(QObject *parent) : QAbstractListModel(parent)
{
}

int RunningAppsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_appPaths.size();
}

QVariant RunningAppsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    const QString &appPath = m_appPaths.at(index.row());
    switch (role) {
    case AppNameRole: return QFileInfo(appPath).fileName();
    case AppPathRole: return QDir::toNativeSeparators(appPath);
    case IsAppSelectedRole: return m_selectedAppIndexes.contains(index.row());
    }

    return QVariant();
}

int RunningAppsModel::selectedCount() const
{
    return m_selectedAppIndexes.size();
}

void RunningAppsModel::selectedStateChanged(const int index, const bool selected)
{
    if (selected) {
        m_selectedAppIndexes.insert(index);
    } else {
        m_selectedAppIndexes.remove(index);
    }
    emit selectedCountChanged();
}

QStringList RunningAppsModel::getSelectedAppPaths()
{
    QStringList appPaths;
    for (const int i : std::as_const(m_selectedAppIndexes)) {
        appPaths.append(m_appPaths.at(i));
    }

    m_selectedAppIndexes.clear();
    emit selectedCountChanged();
    return appPaths;
}

void RunningAppsModel::updateModel()
{
    QStringList appPaths = runningApps::paths();
    std::sort(appPaths.begin(), appPaths.end(), [](const QString &left, const QString &right) {
        return QFileInfo(left).fileName().compare(QFileInfo(right).fileName(), Qt::CaseInsensitive) < 0;
    });

    beginResetModel();
    m_appPaths = appPaths;
    m_selectedAppIndexes.clear();
    endResetModel();
    emit selectedCountChanged();
}

QHash<int, QByteArray> RunningAppsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AppNameRole] = "appName";
    roles[AppPathRole] = "appPath";
    roles[IsAppSelectedRole] = "isAppSelected";
    return roles;
}
