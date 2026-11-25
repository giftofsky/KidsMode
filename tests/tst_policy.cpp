#include "policy.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestPolicy : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void missingFileIsNotOk();
    void malformedFileIsNotOk();
    void startMenuParsing();
};

void TestPolicy::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("policy.json"));

    Policy policy;
    policy.budgetSeconds = 2700;
    policy.pinSalt = QByteArray(16, '\x01');
    policy.pinHash = QByteArray(32, '\x02');
    policy.pinIterations = 100000;
    policy.apps = {AppTile{QStringLiteral("Game"), QStringLiteral("C:/Games/game.exe"),
                           QStringLiteral("-fullscreen")}};

    QVERIFY(policy.save(path));

    bool ok = false;
    const Policy loaded = Policy::load(path, &ok);
    QVERIFY(ok);
    QCOMPARE(loaded.budgetSeconds, 2700);
    QCOMPARE(loaded.pinSalt, policy.pinSalt);
    QCOMPARE(loaded.pinHash, policy.pinHash);
    QCOMPARE(loaded.pinIterations, 100000);
    QCOMPARE(loaded.apps.size(), 1);
    QCOMPARE(loaded.apps.at(0).name, QStringLiteral("Game"));
    QCOMPARE(loaded.apps.at(0).exePath, QStringLiteral("C:/Games/game.exe"));
    QCOMPARE(loaded.apps.at(0).args, QStringLiteral("-fullscreen"));
}

void TestPolicy::missingFileIsNotOk()
{
    QTemporaryDir dir;
    bool ok = true;
    Policy::load(dir.filePath(QStringLiteral("nope.json")), &ok);
    QVERIFY(!ok);
}

void TestPolicy::malformedFileIsNotOk()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("broken.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not json");
    file.close();

    bool ok = true;
    Policy::load(path, &ok);
    QVERIFY(!ok);
}

void TestPolicy::startMenuParsing()
{
    const QString output = QStringLiteral(
        "Coloring Games\tRVAppStudios.Coloring_f5kf90dqd2yjg!App\n"
        "Paint.NET\tD:\\Program Files\\Paint.NET\\paintdotnet.exe\n"
        "Google Chrome\tChrome\n"
        "Control Panel\tMicrosoft.Windows.ControlPanel\n"
        "Uninstall Old App\tC:\\Old\\uninstall.exe\n"
        "Paint\tC:\\Program Files\\Paint\\mspaint.exe\n"
        "broken line without tab\n");

    const QList<AppTile> apps = parseStartMenuApps(output);
    QCOMPARE(apps.size(), 3);
    QCOMPARE(apps.at(0).name, QStringLiteral("Coloring Games"));
    QCOMPARE(apps.at(0).exePath, QStringLiteral("C:\\Windows\\explorer.exe"));
    QCOMPARE(apps.at(0).args,
             QStringLiteral("shell:AppsFolder\\RVAppStudios.Coloring_f5kf90dqd2yjg!App"));
    QCOMPARE(apps.at(1).name, QStringLiteral("Paint"));
    QCOMPARE(apps.at(1).exePath, QDir::toNativeSeparators(QStringLiteral("C:\\Program Files\\Paint\\mspaint.exe")));
    QVERIFY(apps.at(1).args.isEmpty());
    QCOMPARE(apps.at(2).name, QStringLiteral("Paint.NET"));
}

QTEST_GUILESS_MAIN(TestPolicy)
#include "tst_policy.moc"
