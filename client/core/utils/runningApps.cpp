#include "runningApps.h"

#include <QCoreApplication>
#include <QDir>
#include <QSet>

#ifdef Q_OS_WIN
    #include <windows.h>

    #include <tlhelp32.h>
#endif

QStringList runningApps::paths()
{
    QStringList paths;
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return paths;
    }

    const QString systemRoot = QDir::cleanPath(qEnvironmentVariable("SystemRoot")) + QLatin1Char('/');
    const QString ownDir = QDir::cleanPath(QCoreApplication::applicationDirPath()) + QLatin1Char('/');
    QSet<QString> seenPaths;

    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    for (BOOL hasEntry = Process32FirstW(snapshot, &entry); hasEntry; hasEntry = Process32NextW(snapshot, &entry)) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process == nullptr) {
            continue;
        }

        wchar_t buffer[MAX_PATH * 4];
        DWORD size = static_cast<DWORD>(std::size(buffer));
        const bool hasPath = QueryFullProcessImageNameW(process, 0, buffer, &size);
        CloseHandle(process);
        if (!hasPath) {
            continue;
        }

        const QString path = QDir::cleanPath(QString::fromWCharArray(buffer, static_cast<int>(size)));
        const QString key = path.toLower();
        if (path.startsWith(systemRoot, Qt::CaseInsensitive) || path.startsWith(ownDir, Qt::CaseInsensitive)
            || seenPaths.contains(key)) {
            continue;
        }

        seenPaths.insert(key);
        paths.append(path);
    }

    CloseHandle(snapshot);
#endif
    return paths;
}
