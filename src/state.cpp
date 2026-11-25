#include "state.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

QString UsageState::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/state.json");
}

QJsonObject UsageState::toJson() const
{
    return QJsonObject{
        {QStringLiteral("date"), date.toString(Qt::ISODate)},
        {QStringLiteral("secondsUsed"), secondsUsed},
        {QStringLiteral("bonusSeconds"), bonusSeconds},
        {QStringLiteral("failedPins"), failedPins},
        {QStringLiteral("failedPinUntilEpoch"), static_cast<double>(failedPinUntilEpoch)},
        {QStringLiteral("childPid"), static_cast<double>(childPid)},
        {QStringLiteral("childExe"), childExe},
    };
}

UsageState UsageState::fromJson(const QJsonObject &object)
{
    UsageState state;
    state.date = QDate::fromString(object.value(QStringLiteral("date")).toString(), Qt::ISODate);
    state.secondsUsed = object.value(QStringLiteral("secondsUsed")).toInt();
    state.bonusSeconds = object.value(QStringLiteral("bonusSeconds")).toInt();
    state.failedPins = object.value(QStringLiteral("failedPins")).toInt();
    state.failedPinUntilEpoch =
        static_cast<qint64>(object.value(QStringLiteral("failedPinUntilEpoch")).toDouble());
    state.childPid = static_cast<qint64>(object.value(QStringLiteral("childPid")).toDouble());
    state.childExe = object.value(QStringLiteral("childExe")).toString();
    return state;
}

UsageState UsageState::load(const QString &path, bool *fileExisted)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (fileExisted)
            *fileExisted = false;
        return UsageState{};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (fileExisted)
        *fileExisted = true;

    if (!document.isObject())
        return UsageState{};

    return fromJson(document.object());
}

bool UsageState::save(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return file.commit();
}

void UsageState::rollToDate(const QDate &today)
{
    if (date == today)
        return;
    date = today;
    secondsUsed = 0;
    bonusSeconds = 0;
}

bool UsageState::isLockedOutAt(qint64 nowEpoch) const
{
    return failedPinUntilEpoch > nowEpoch;
}
