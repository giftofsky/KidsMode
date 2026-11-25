#include "state.h"

#include <QTemporaryDir>
#include <QTest>

namespace {
const QDate kToday(2026, 11, 25);
const qint64 kNow = 1795600000;

UsageState sampleState()
{
    UsageState state;
    state.date = kToday;
    state.secondsUsed = 1234;
    state.bonusSeconds = 900;
    state.failedPins = 2;
    state.failedPinUntilEpoch = kNow + 60;
    state.childPid = 4242;
    state.childExe = QStringLiteral("C:/Games/game.exe");
    return state;
}
} // namespace

class TestState : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void missingFileIsEmpty();
    void rollToDateResetsUsageAndBonus();
    void rollToDateKeepsSameDay();
    void lockoutWindow();
};

void TestState::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("state.json"));

    const UsageState original = sampleState();
    QVERIFY(original.save(path));

    bool existed = false;
    const UsageState loaded = UsageState::load(path, &existed);
    QVERIFY(existed);
    QCOMPARE(loaded.date, original.date);
    QCOMPARE(loaded.secondsUsed, original.secondsUsed);
    QCOMPARE(loaded.bonusSeconds, original.bonusSeconds);
    QCOMPARE(loaded.failedPins, original.failedPins);
    QCOMPARE(loaded.failedPinUntilEpoch, original.failedPinUntilEpoch);
    QCOMPARE(loaded.childPid, original.childPid);
    QCOMPARE(loaded.childExe, original.childExe);
}

void TestState::missingFileIsEmpty()
{
    QTemporaryDir dir;
    bool existed = true;
    const UsageState state = UsageState::load(dir.filePath(QStringLiteral("missing.json")), &existed);
    QVERIFY(!existed);
    QCOMPARE(state.secondsUsed, 0);
    QCOMPARE(state.bonusSeconds, 0);
    QVERIFY(!state.isLockedOutAt(kNow));
}

void TestState::rollToDateResetsUsageAndBonus()
{
    UsageState state = sampleState();
    state.date = kToday.addDays(-1);

    state.rollToDate(kToday);

    QCOMPARE(state.date, kToday);
    QCOMPARE(state.secondsUsed, 0);
    QCOMPARE(state.bonusSeconds, 0);
}

void TestState::rollToDateKeepsSameDay()
{
    UsageState state = sampleState();
    state.rollToDate(kToday);
    QCOMPARE(state.secondsUsed, 1234);
    QCOMPARE(state.bonusSeconds, 900);
}

void TestState::lockoutWindow()
{
    UsageState state = sampleState();
    QVERIFY(state.isLockedOutAt(kNow + 30));
    QVERIFY(!state.isLockedOutAt(kNow + 61));
}

QTEST_GUILESS_MAIN(TestState)
#include "tst_state.moc"
