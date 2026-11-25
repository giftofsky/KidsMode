#include "sessiontimer.h"

SessionTimer::SessionTimer(Clock clock, QObject *parent)
    : QObject(parent)
    , m_clock(clock ? std::move(clock) : Clock([] { return QDateTime::currentDateTime(); }))
    , m_date(m_clock().date())
{
}

void SessionTimer::setBudgetSeconds(int seconds)
{
    m_budgetSeconds = qMax(0, seconds);
    // Re-arm both signals: a parent top-up must be able to warn and exhaust again.
    m_exhaustedEmitted = false;
    m_warn5Emitted = remainingSeconds() <= 300;
    m_warn1Emitted = remainingSeconds() <= 60;
}

int SessionTimer::remainingSeconds() const
{
    return qMax(0, m_budgetSeconds - m_secondsUsed);
}

bool SessionTimer::isExhausted() const
{
    return m_secondsUsed >= m_budgetSeconds;
}

void SessionTimer::setRunning(bool running)
{
    m_running = running;
}

void SessionTimer::restore(int secondsUsed, const QDate &date)
{
    m_secondsUsed = qMax(0, secondsUsed);
    m_date = date.isValid() ? date : m_clock().date();
    m_exhaustedEmitted = isExhausted();
    m_warn5Emitted = remainingSeconds() <= 300;
    m_warn1Emitted = remainingSeconds() <= 60;
}

void SessionTimer::clearWarningFlags()
{
    m_exhaustedEmitted = false;
    m_warn5Emitted = false;
    m_warn1Emitted = false;
}

void SessionTimer::tick()
{
    if (!m_running)
        return;

    const QDateTime now = m_clock();

    if (now.date() != m_date) {
        m_date = now.date();
        m_secondsUsed = 0;
        clearWarningFlags();
        emit dayRolledOver();
    }

    if (!isExhausted()) {
        ++m_secondsUsed;
        emitWarningsIfNeeded();
    }

    if (isExhausted() && !m_exhaustedEmitted) {
        m_exhaustedEmitted = true;
        m_running = false;
        emit exhausted();
    }
}

void SessionTimer::emitWarningsIfNeeded()
{
    const int left = remainingSeconds();
    if (left <= 0)
        return;

    if (left <= 60 && !m_warn1Emitted) {
        m_warn1Emitted = true;
        emit warning(left);
    } else if (left <= 300 && !m_warn5Emitted) {
        m_warn5Emitted = true;
        emit warning(left);
    }
}
