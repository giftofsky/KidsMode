#pragma once

#include <QDate>
#include <QObject>

#include <functional>

// Counts only while the state machine says the kid is active.
// The clock is injected so tests never depend on wall time.
class SessionTimer : public QObject
{
    Q_OBJECT
public:
    using Clock = std::function<QDateTime()>;

    explicit SessionTimer(Clock clock = {}, QObject *parent = nullptr);

    void setBudgetSeconds(int seconds);
    int budgetSeconds() const { return m_budgetSeconds; }

    int secondsUsed() const { return m_secondsUsed; }
    int remainingSeconds() const;
    bool isExhausted() const;

    QDate date() const { return m_date; }

    void setRunning(bool running);
    bool isRunning() const { return m_running; }

    void restore(int secondsUsed, const QDate &date);

public slots:
    void tick();

signals:
    void warning(int secondsLeft);
    void exhausted();
    void dayRolledOver();

private:
    void emitWarningsIfNeeded();
    void clearWarningFlags();

    Clock m_clock;
    int m_budgetSeconds = 0;
    int m_secondsUsed = 0;
    QDate m_date;
    bool m_running = false;
    bool m_exhaustedEmitted = false;
    bool m_warn5Emitted = false;
    bool m_warn1Emitted = false;
};
