#include "folderExecutables.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

#include <vector>

#ifdef Q_OS_WIN
    #include <windows.h>
#endif

bool folderExecutables::isLocalFolder(const QString &folderPath)
{
    const QString cleanPath = QDir::cleanPath(folderPath);
    if (cleanPath.size() < 2 || !cleanPath.at(0).isLetter() || cleanPath.at(1) != QLatin1Char(':')) {
        return false;
    }
#ifdef Q_OS_WIN
    const QString driveRoot = cleanPath.left(2) + QLatin1Char('\\');
    return GetDriveTypeW(reinterpret_cast<const wchar_t *>(driveRoot.utf16())) != DRIVE_REMOTE;
#else
    return true;
#endif
}

namespace
{
    QString realFolderPath(const QString &folderPath)
    {
#ifdef Q_OS_WIN
        const QString nativePath = QDir::toNativeSeparators(QDir::cleanPath(folderPath));
        HANDLE handle = CreateFileW(reinterpret_cast<const wchar_t *>(nativePath.utf16()), 0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            constexpr DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
            std::vector<wchar_t> buffer(MAX_PATH);
            DWORD length = GetFinalPathNameByHandleW(handle, buffer.data(), DWORD(buffer.size()), flags);
            if (length >= buffer.size()) {
                buffer.resize(length);
                length = GetFinalPathNameByHandleW(handle, buffer.data(), DWORD(buffer.size()), flags);
            }
            CloseHandle(handle);

            if (length > 0 && length < buffer.size()) {
                const QString finalPath = QString::fromWCharArray(buffer.data(), length);
                const QLatin1String localPrefix("\\\\?\\");
                if (finalPath.startsWith(localPrefix) && !finalPath.startsWith(QLatin1String("\\\\?\\UNC\\"))) {
                    return QDir::cleanPath(QDir::fromNativeSeparators(finalPath.mid(localPrefix.size())));
                }
            }
        }
#endif
        return QDir::cleanPath(folderPath);
    }
}

QStringList folderExecutables::find(const QString &folderPath)
{
    QStringList executables;
    QStringList pendingDirs { realFolderPath(folderPath) };

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

    if (executables.size() >= maxExecutables) {
        qWarning() << QString("App split tunneling: scan of %1 stopped at %2 executables")
                              .arg(QDir::toNativeSeparators(folderPath))
                              .arg(maxExecutables);
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
