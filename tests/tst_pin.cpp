#include "pin.h"

#include <QTest>

class TestPin : public QObject
{
    Q_OBJECT

private slots:
    void createVerify();
    void wrongPinRejected();
    void corruptHashRejected();
    void lockoutSchedule();
};

void TestPin::createVerify()
{
    const Pin::Hash hash = Pin::create(QStringLiteral("1234"), 1000);
    QCOMPARE(hash.salt.size(), 16);
    QCOMPARE(hash.hash.size(), 32);
    QVERIFY(Pin::verify(QStringLiteral("1234"), hash.salt, hash.hash, hash.iterations));
}

void TestPin::wrongPinRejected()
{
    const Pin::Hash hash = Pin::create(QStringLiteral("1234"), 1000);
    QVERIFY(!Pin::verify(QStringLiteral("1235"), hash.salt, hash.hash, hash.iterations));
    QVERIFY(!Pin::verify(QString(), hash.salt, hash.hash, hash.iterations));
}

void TestPin::corruptHashRejected()
{
    QVERIFY(!Pin::verify(QStringLiteral("1234"), QByteArray(), QByteArray(), 1000));
    QVERIFY(!Pin::verify(QStringLiteral("1234"), QByteArray(16, 'a'), QByteArray(32, 'b'), 1000));
}

void TestPin::lockoutSchedule()
{
    QCOMPARE(Pin::lockoutSecondsAfterFailure(0), 0);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(4), 0);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(5), 300);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(6), 600);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(7), 1200);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(8), 2400);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(9), 3600);
    QCOMPARE(Pin::lockoutSecondsAfterFailure(50), 3600);
}

QTEST_GUILESS_MAIN(TestPin)
#include "tst_pin.moc"
