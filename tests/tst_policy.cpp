#include "policy.h"

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

QTEST_GUILESS_MAIN(TestPolicy)
#include "tst_policy.moc"
