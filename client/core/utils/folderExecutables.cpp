#include "folderExecutables.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

QStringList folderExecutables::find(const QString &folderPath)
{
    QStringList executables;
    QStringList pendingDirs { QDir::cleanPath(folderPath) };

    while (!pendingDirs.isEmpty() && executables.size() < maxExecutables) {
        QDirIterator it(pendingDirs.takeLast(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (it.hasNext() && executables.size() < maxExecutables) {
            const QFileInfo info = it.nextFileInfo();
            if (info.isSymLink() || info.isJunction()) {
                continue;
            }

            if (info.isDir()) {
                pendingDirs.append(info.filePath());
            } else if (info.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0) {
                executables.append(info.filePath());
            }
        }
    }

    return executables;
}

QStringList folderExecutables::expand(const QStringList &appPaths, const QStringList &folderPaths)
{
    QStringList result;
    QSet<QString> seen;

    auto append = [&result, &seen](const QString &path) {
        const QString key = QDir::cleanPath(path).toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            result.append(path);
        }
    };

    for (const auto &path : appPaths) {
        append(path);
    }
    for (const auto &folder : folderPaths) {
        for (const auto &path : find(folder)) {
            append(path);
        }
    }

    return result;
}
