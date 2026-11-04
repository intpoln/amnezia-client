#ifndef RUNNINGAPPSMODEL_H
#define RUNNINGAPPSMODEL_H

#include <QAbstractListModel>
#include <QSet>
#include <QStringList>

class RunningAppsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

public:
    enum Roles {
        AppNameRole = Qt::UserRole + 1,
        AppPathRole,
        IsAppSelectedRole
    };

    explicit RunningAppsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    int selectedCount() const;

public slots:
    void selectedStateChanged(const int index, const bool selected);
    QStringList getSelectedAppPaths() const;

    void updateModel();

signals:
    void selectedCountChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    QStringList m_appPaths;
    QSet<int> m_selectedAppIndexes;
};

#endif // RUNNINGAPPSMODEL_H
