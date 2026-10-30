#ifndef FOLDEREXECUTABLES_H
#define FOLDEREXECUTABLES_H

#include <QString>
#include <QStringList>

namespace folderExecutables
{
    constexpr const int maxExecutables = 10000;

    QStringList find(const QString &folderPath);
    QStringList expand(const QStringList &appPaths, const QStringList &folderPaths);
};

#endif // FOLDEREXECUTABLES_H
