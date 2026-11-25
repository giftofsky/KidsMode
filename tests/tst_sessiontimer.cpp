#include "sessiontimer.h"

#include <QSignalSpy>
#include <QTest>

namespace {

struct FakeClock
{
    QDateTime now{QDate(2026, 11, 25), QTime(10, 0)};
};

void advance(FakeClock &clock, int seconds = 1)
{
    clock.now = clock.now.addSecs(seconds);
}

} // namespace

class TestSessionTimer : public QObject
{
    Q_OBJECT

private slots:
    void countsOnlyWhileRunning();
    void exhaustsExactlyAtBudgetOnce();
    void warnsAtFiveAndOneMinute();
    void dayRolloverResets();
    void topUpReArmsAndExhaustsAgain();
};

void TestSessionTimer::countsOnlyWhileRunning()
{
    FakeClock clock;
    SessionTimer timer([&] { return clock.now; });
    timer.setBudgetSeconds(100);

    advance(clock);
    timer.tick();
    QCOMPARE(timer.secondsUsed(), 0);

    timer.setRunning(true);
    advance(clock);
    timer.tick();
    QCOMPARE(timer.secondsUsed(), 1);

    timer.setRunning(false);
    advance(clock);
    timer.tick();
    QCOMPARE(timer.secondsUsed(), 1);
}

void TestSessionTimer::exhaustsExactlyAtBudgetOnce()
{
    FakeClock clock;
    SessionTimer timer([&] { return clock.now; });
    timer.setBudgetSeconds(3);
    QSignalSpy exhausted(&timer, &SessionTimer::exhausted);

    timer.setRunning(true);
    for (int i = 0; i < 2; ++i) {
        advance(clock);
        timer.tick();
    }
    QCOMPARE(timer.secondsUsed(), 2);
    QCOMPARE(timer.remainingSeconds(), 1);
    QCOMPARE(exhausted.count(), 0);

    advance(clock);
    timer.tick();
    QCOMPARE(timer.secondsUsed(), 3);
    QCOMPARE(exhausted.count(), 1);
    QVERIFY(timer.isExhausted());
    QVERIFY(!timer.isRunning());

    advance(clock);
    timer.tick();
    QCOMPARE(exhausted.count(), 1);
}

void TestSessionTimer::warnsAtFiveAndOneMinute()
{
    FakeClock clock;
    SessionTimer timer([&] { return clock.now; });
    timer.setBudgetSeconds(400);
    timer.setRunning(true);

    QList<int> warnings;
    connect(&timer, &SessionTimer::warning, this, [&](int left) { warnings.append(left); });

    for (int i = 0; i < 400; ++i) {
        advance(clock);
        timer.tick();
    }

    QCOMPARE(warnings.size(), 2);
    QCOMPARE(warnings.at(0), 300);
    QCOMPARE(warnings.at(1), 60);
}

void TestSessionTimer::dayRolloverResets()
{
    FakeClock clock;
    SessionTimer timer([&] { return clock.now; });
    timer.setBudgetSeconds(100);
    timer.setRunning(true);
    QSignalSpy rolledOver(&timer, &SessionTimer::dayRolledOver);

    for (int i = 0; i < 3; ++i) {
        advance(clock);
        timer.tick();
    }
    QCOMPARE(timer.secondsUsed(), 3);

    clock.now = QDateTime(QDate(2026, 11, 26), QTime(0, 0, 1));
    timer.tick();

    QCOMPARE(rolledOver.count(), 1);
    QCOMPARE(timer.secondsUsed(), 1);
    QCOMPARE(timer.date(), QDate(2026, 11, 26));
}

void TestSessionTimer::topUpReArmsAndExhaustsAgain()
{
    FakeClock clock;
    SessionTimer timer([&] { return clock.now; });
    timer.setBudgetSeconds(400);
    timer.setRunning(true);

    QList<int> warnings;
    connect(&timer, &SessionTimer::warning, this, [&](int left) { warnings.append(left); });
    QSignalSpy exhausted(&timer, &SessionTimer::exhausted);

    for (int i = 0; i < 400; ++i) {
        advance(clock);
        timer.tick();
    }
    QCOMPARE(exhausted.count(), 1);
    QVERIFY(!timer.isRunning());
    QCOMPARE(warnings.size(), 2);
    QCOMPARE(warnings.last(), 60);

    // The parent typed the PIN and chose "Add 15 minutes".
    timer.setBudgetSeconds(timer.budgetSeconds() + 900);
    timer.setRunning(true);
    QCOMPARE(timer.remainingSeconds(), 900);

    for (int i = 0; i < 600; ++i) {
        advance(clock);
        timer.tick();
    }
    QCOMPARE(timer.remainingSeconds(), 300);
    QCOMPARE(warnings.last(), 300);

    for (int i = 0; i < 300; ++i) {
        advance(clock);
        timer.tick();
    }
    QCOMPARE(exhausted.count(), 2);
    QCOMPARE(timer.remainingSeconds(), 0);
    QVERIFY(!timer.isRunning());
}

QTEST_GUILESS_MAIN(TestSessionTimer)
#include "tst_sessiontimer.moc"
