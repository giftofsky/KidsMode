#include "policy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

QString Policy::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/policy.json");
}

QJsonObject Policy::toJson() const
{
    QJsonArray tileArray;
    for (const AppTile &tile : apps) {
        tileArray.append(QJsonObject{
            {QStringLiteral("name"), tile.name},
            {QStringLiteral("exePath"), tile.exePath},
            {QStringLiteral("args"), tile.args},
        });
    }

    return QJsonObject{
        {QStringLiteral("budgetSeconds"), budgetSeconds},
        {QStringLiteral("pinSalt"), QString::fromLatin1(pinSalt.toBase64())},
        {QStringLiteral("pinHash"), QString::fromLatin1(pinHash.toBase64())},
        {QStringLiteral("pinIterations"), pinIterations},
        {QStringLiteral("apps"), tileArray},
    };
}

Policy Policy::fromJson(const QJsonObject &object)
{
    Policy policy;
    policy.budgetSeconds = object.value(QStringLiteral("budgetSeconds")).toInt(3600);
    policy.pinSalt = QByteArray::fromBase64(object.value(QStringLiteral("pinSalt")).toString().toLatin1());
    policy.pinHash = QByteArray::fromBase64(object.value(QStringLiteral("pinHash")).toString().toLatin1());
    policy.pinIterations = object.value(QStringLiteral("pinIterations")).toInt(100000);

    const QJsonArray tileArray = object.value(QStringLiteral("apps")).toArray();
    for (const QJsonValue &value : tileArray) {
        const QJsonObject tileObject = value.toObject();
        policy.apps.append(AppTile{
            tileObject.value(QStringLiteral("name")).toString(),
            tileObject.value(QStringLiteral("exePath")).toString(),
            tileObject.value(QStringLiteral("args")).toString(),
        });
    }

    return policy;
}

Policy Policy::load(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return Policy{};
    }

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (ok)
            *ok = false;
        return Policy{};
    }

    if (ok)
        *ok = true;
    return fromJson(document.object());
}

bool Policy::save(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return file.commit();
}
