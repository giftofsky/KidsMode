#pragma once

#include <QDate>
#include <QJsonObject>
#include <QString>

// The kid can edit this (it lives in AppData and is written unelevated).
// It is a usage record, not evidence — see AGENTS.md.
struct UsageState
{
    QDate date;
    int secondsUsed = 0;
    int bonusSeconds = 0;
    int failedPins = 0;
    qint64 failedPinUntilEpoch = 0;
    qint64 childPid = 0;
    QString childExe;

    static UsageState load(const QString &path, bool *fileExisted = nullptr);
    bool save(const QString &path) const;

    QJsonObject toJson() const;
    static UsageState fromJson(const QJsonObject &object);

    static QString defaultPath();

    // New local date: today's usage and top-ups start from zero.
    void rollToDate(const QDate &today);
    bool isLockedOutAt(qint64 nowEpoch) const;
};
