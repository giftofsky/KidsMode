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
    void friendlyNames();
    void shownNameOverride();
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

void TestPolicy::friendlyNames()
{
    QCOMPARE(friendlyName(QStringLiteral("Math Kids: Math Games For Kids")),
             QStringLiteral("Math Kids"));
    QCOMPARE(friendlyName(QStringLiteral("Coloring Games: Coloring Book & Painting")),
             QStringLiteral("Coloring Games"));
    QCOMPARE(friendlyName(QStringLiteral("Candy Swipe for Kids Free")),
             QStringLiteral("Candy Swipe"));
    QCOMPARE(friendlyName(QStringLiteral("Solitaire \u2013 Casual Card Games")),
             QStringLiteral("Solitaire"));
    QCOMPARE(friendlyName(QStringLiteral("  Wild   Spaces  here ")),
             QStringLiteral("Wild Spaces here"));
    // nothing to take away, and a one-word name is never eaten by the boilerplate strip
    QCOMPARE(friendlyName(QStringLiteral("Minecraft Education Edition")),
             QStringLiteral("Minecraft Education Edition"));
    QCOMPARE(friendlyName(QStringLiteral("Free")), QStringLiteral("Free"));
    // an all-lower-case name is a shortcut, not a label; anything already cased is untouched
    QCOMPARE(friendlyName(QStringLiteral("chrome")), QStringLiteral("Chrome"));
    QCOMPARE(friendlyName(QStringLiteral("paint net")), QStringLiteral("Paint Net"));
    QCOMPARE(friendlyName(QStringLiteral("iPhone Games")), QStringLiteral("iPhone Games"));
    QCOMPARE(friendlyName(QStringLiteral("Paint.NET")), QStringLiteral("Paint.NET"));
    QVERIFY(friendlyName(QString()).isEmpty());
}

void TestPolicy::shownNameOverride()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("policy.json"));

    Policy policy;
    policy.pinHash = QByteArray(32, '\x02');
    policy.apps = {
        AppTile{QStringLiteral("Coloring Games: Coloring Book & Painting"),
                QStringLiteral("C:/a.exe"), QString(), QStringLiteral("Coloring & Painting")},
        AppTile{QStringLiteral("Math Kids: Math Games For Kids"), QStringLiteral("C:/b.exe")},
    };
    QVERIFY(policy.save(path));

    const Policy loaded = Policy::load(path);
    QCOMPARE(loaded.apps.size(), 2);
    QCOMPARE(loaded.apps.at(0).label(), QStringLiteral("Coloring & Painting"));
    QCOMPARE(loaded.apps.at(1).shownName, QString());
    QCOMPARE(loaded.apps.at(1).label(), QStringLiteral("Math Kids"));
}

QTEST_GUILESS_MAIN(TestPolicy)
#include "tst_policy.moc"
