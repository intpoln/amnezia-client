#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QTest>
#include <QUrl>

#include "utils/testCoreController.h"
#include "core/models/serverDescription.h"
#include "core/utils/folderExecutables.h"
#include "secureQSettings.h"
#include "vpnConnection.h"

using namespace amnezia;

class TestUiAppSTModelAndController : public QObject
{
    Q_OBJECT

private:
    TestCoreController *m_coreController;
    SecureQSettings *m_settings;

private slots:
    void initTestCase()
    {
        QString testOrg = "AmneziaVPN-Test-" + QUuid::createUuid().toString();
        m_settings = new SecureQSettings(testOrg, "amnezia-client", nullptr, false);

        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);

        m_coreController = new TestCoreController(vpnConnection, m_settings, nullptr, this);
    }

    void cleanupTestCase()
    {
        m_settings->clearSettings();
        delete m_coreController;
        delete m_settings;
    }

    void init()
    {
        m_settings->clearSettings();
        if (m_coreController->m_serversModel) {
            m_coreController->m_serversModel->updateModel(QVector<ServerDescription>(), QString{});
        }
    }

    void testRolesAndSignals()
    {
        QSignalSpy finishedSpy(m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::finished);
        QSignalSpy errorOccurredSpy(m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::errorOccurred);
        QSignalSpy isSplitTunnelingChangedSpy(m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::isSplitTunnelingEnabledChanged);

        m_coreController->m_appSplitTunnelingUiController->toggleSplitTunneling(true);
        QVERIFY2(isSplitTunnelingChangedSpy.count() == 1, "isSplitTunnelingChangedSpy signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingUiController->isSplitTunnelingEnabled() == true, "AppSplitTunneling should be enabled");

        m_coreController->m_appSplitTunnelingUiController->toggleSplitTunneling(false);
        QVERIFY2(isSplitTunnelingChangedSpy.count() == 2, "isSplitTunnelingChangedSpy signal should be emitted 2nd time");
        QVERIFY2(m_coreController->m_appSplitTunnelingUiController->isSplitTunnelingEnabled() == false, "AppSplitTunneling should be disabled");

        const QString app = QCoreApplication::applicationFilePath();

        m_coreController->m_appSplitTunnelingUiController->addApp(app);
        m_coreController->m_appSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 1, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingModel->rowCount() == 1, "AppSplitTunnelingModel should have 1 row");

        QModelIndex appSTModelIndex = m_coreController->m_appSplitTunnelingModel->index(0, 0);
        QVERIFY2(appSTModelIndex.isValid(), "Site model index should be valid");

        auto appPath = m_coreController->m_appSplitTunnelingModel->data(appSTModelIndex, AppSplitTunnelingModel::AppPathRole);
        QString msg = QString("app path should be %1, got %2").arg(app, appPath.toString());
        QVERIFY2(app.contains(appPath.toString()) == true, msg.toLocal8Bit().constData());

        auto pkgAppName = m_coreController->m_appSplitTunnelingModel->data(appSTModelIndex, AppSplitTunnelingModel::PackageAppNameRole);
        QVERIFY2(pkgAppName == true, "app name should be set");

        auto pkgAppIcon = m_coreController->m_appSplitTunnelingModel->data(appSTModelIndex, AppSplitTunnelingModel::PackageAppIconRole);
        QVERIFY2(pkgAppIcon == true, "app image should be set");

        m_coreController->m_appSplitTunnelingUiController->addApp(app);
        m_coreController->m_appSplitTunnelingUiController->updateModel();
        QVERIFY2(errorOccurredSpy.count() == 1, "errorOccurred signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingModel->rowCount() == 1, "AppSplitTunnelingModel should have 3 rows (same app should not be added)");

        m_coreController->m_appSplitTunnelingUiController->removeApp(0);
        m_coreController->m_appSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 2, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingModel->rowCount() == 0, "AppSplitTunnelingModel should have 0 rows");
    }

    void testFolderExecutables()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath("sub/deep"));
        for (const QString &name : { "a.exe", "sub/b.EXE", "sub/c.dll", "sub/deep/d.exe", "readme.txt" }) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        QStringList found = folderExecutables::find(dir.path());
        found.sort(Qt::CaseInsensitive);
        QCOMPARE(found,
                 QStringList({ dir.filePath("a.exe"), dir.filePath("sub/b.EXE"), dir.filePath("sub/deep/d.exe") }));

        const QStringList expanded =
                folderExecutables::expand({ dir.filePath("A.EXE"), "C:/Other/app.exe" }, { dir.path() });
        QCOMPARE(expanded.size(), 4);
        QCOMPARE(expanded.first(), dir.filePath("A.EXE"));

        QVERIFY(folderExecutables::find(dir.filePath("missing")).isEmpty());

        QVERIFY(folderExecutables::isLocalFolder(dir.path()));
        QVERIFY(folderExecutables::isLocalFolder("d:/Games"));
        QVERIFY(!folderExecutables::isLocalFolder("//server/share/Games"));
        QVERIFY(!folderExecutables::isLocalFolder("\\\\server\\share"));
        QVERIFY(!folderExecutables::isLocalFolder("Games"));
    }

#ifdef Q_OS_WIN
    void testFolderExecutablesJunctionRoot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath("target"));
        QFile exe(dir.filePath("target/game.exe"));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();

        const QString link = dir.filePath("link");
        const QStringList mklinkArgs { "/c", "mklink", "/J", QDir::toNativeSeparators(link),
                                       QDir::toNativeSeparators(dir.filePath("target")) };
        QCOMPARE(QProcess::execute("cmd", mklinkArgs), 0);

        const QString target = QFileInfo(dir.filePath("target")).canonicalFilePath();
        QCOMPARE(folderExecutables::find(link), QStringList({ target + "/game.exe" }));
    }
#endif

    void testFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile exe(dir.filePath("game.exe"));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();

        auto uiController = m_coreController->m_appSplitTunnelingUiController;
        auto model = m_coreController->m_appSplitTunnelingModel;
        QSignalSpy finishedSpy(uiController, &AppSplitTunnelingUiController::finished);
        QSignalSpy errorOccurredSpy(uiController, &AppSplitTunnelingUiController::errorOccurred);

        uiController->addApp(QCoreApplication::applicationFilePath());
        uiController->addFolder(QUrl::fromLocalFile(dir.path()));
        QVERIFY(finishedSpy.wait(5000) || finishedSpy.count() == 2);
        QCOMPARE(finishedSpy.count(), 2);
        QVERIFY2(finishedSpy.last().first().toString().endsWith(": 1"),
                 qPrintable(finishedSpy.last().first().toString()));

        uiController->updateModel();
        QCOMPARE(model->rowCount(), 2);
        const QModelIndex folderIndex = model->index(1, 0);
        QCOMPARE(model->data(folderIndex, AppSplitTunnelingModel::IsFolderRole).toBool(), true);
        QCOMPARE(model->data(folderIndex, AppSplitTunnelingModel::FullPathRole).toString(),
                 QDir::toNativeSeparators(dir.path()));
        QCOMPARE(model->data(folderIndex, AppSplitTunnelingModel::AppPathRole).toString(),
                 QFileInfo(dir.path()).fileName());
        QCOMPARE(model->data(model->index(0, 0), AppSplitTunnelingModel::IsFolderRole).toBool(), false);

        uiController->addFolder(QUrl::fromLocalFile(dir.path().toUpper()));
        QCOMPARE(errorOccurredSpy.count(), 1);

        uiController->addFolder(QUrl::fromLocalFile("//server/share/Games"));
        QCOMPARE(errorOccurredSpy.count(), 2);

        auto repository = m_coreController->m_appSettingsRepository;
        QCOMPARE(repository->vpnAppFolders(AppsRouteMode::VpnAllExceptApps), QStringList({ dir.path() }));
        QCOMPARE(repository->vpnApps(AppsRouteMode::VpnAllExceptApps).size(), 1);
        QCOMPARE(AppSplitTunnelingController(repository).getFolders(), QStringList({ dir.path() }));

        uiController->removeApp(1);
        uiController->updateModel();
        QCOMPARE(model->rowCount(), 1);
        QVERIFY(repository->vpnAppFolders(AppsRouteMode::VpnAllExceptApps).isEmpty());
        QCOMPARE(repository->vpnApps(AppsRouteMode::VpnAllExceptApps).size(), 1);

        uiController->removeApp(0);
    }

    void testFoldersClearedWithAppsList()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto controller = m_coreController->m_appSplitTunnelingController;
        auto repository = m_coreController->m_appSettingsRepository;
        QVERIFY(controller->addFolder(dir.path()));

        controller->clearAppsList();
        QVERIFY(controller->getFolders().isEmpty());
        QVERIFY(repository->vpnAppFolders(AppsRouteMode::VpnAllExceptApps).isEmpty());
    }

    void testFoldersOnRestore()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto repository = m_coreController->m_appSettingsRepository;
        repository->setVpnAppFolders(AppsRouteMode::VpnAllExceptApps, { dir.path() });
        const QByteArray backupWithFolders = repository->backupAppConfig();
        QVERIFY(QJsonDocument::fromJson(backupWithFolders).object().contains("Conf/ExceptAppsFolders"));

        const QByteArray backupWithoutFolders =
                QJsonDocument(QJsonObject { { "Conf/appsSplitTunnelingEnabled", true } }).toJson();
        QVERIFY(repository->restoreAppConfig(backupWithoutFolders));
        QVERIFY(repository->vpnAppFolders(AppsRouteMode::VpnAllExceptApps).isEmpty());

        QVERIFY(repository->restoreAppConfig(backupWithFolders));
        QCOMPARE(repository->vpnAppFolders(AppsRouteMode::VpnAllExceptApps), QStringList({ dir.path() }));
    }

    void testAddAppFiles()
    {
        auto uiController = m_coreController->m_appSplitTunnelingUiController;
        auto repository = m_coreController->m_appSettingsRepository;
        QSignalSpy finishedSpy(uiController, &AppSplitTunnelingUiController::finished);

        uiController->addAppFiles({ QUrl::fromLocalFile("C:/Games/a.exe"), QUrl::fromLocalFile("C:/Games/b.exe") });
        QCOMPARE(repository->vpnApps(AppsRouteMode::VpnAllExceptApps).size(), 2);

        uiController->addAppFiles({ QUrl::fromLocalFile("c:/games/A.EXE"), QUrl::fromLocalFile("C:/Games/c.exe") });
        QCOMPARE(repository->vpnApps(AppsRouteMode::VpnAllExceptApps).size(), 3);
        QCOMPARE(finishedSpy.count(), 2);
        QVERIFY2(finishedSpy.last().first().toString().endsWith(": 1"), qPrintable(finishedSpy.last().first().toString()));

        m_coreController->m_appSplitTunnelingController->clearAppsList();
    }
};

QTEST_MAIN(TestUiAppSTModelAndController)
#include "testUiAppSTModelAndController.moc"
