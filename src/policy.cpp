#include "policy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

QString Policy::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/policy.json");
}

QJsonObject Policy::toJson() const
{
    QJsonArray tileArray;
    for (const AppTile &tile : apps) {
        QJsonObject tileObject{
            {QStringLiteral("name"), tile.name},
            {QStringLiteral("exePath"), tile.exePath},
            {QStringLiteral("args"), tile.args},
        };
        if (!tile.shownName.isEmpty()) // old policy.json files simply have no such key
            tileObject.insert(QStringLiteral("shownName"), tile.shownName);
        tileArray.append(tileObject);
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
            tileObject.value(QStringLiteral("shownName")).toString(),
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

QString friendlyName(const QString &rawName)
{
    // Everything after a colon or a dash is store-page marketing: "Math Kids: Math Games For
    // Kids" → "Math Kids". \x{…} keeps the pattern ASCII.
    static const QRegularExpression separator(QStringLiteral("[\\x{2013}\\x{2014}:]"));
    QString name = rawName.section(separator, 0, 0);
    if (name.trimmed().isEmpty())
        name = rawName;

    // Boilerplate on the end tells a child nothing either. Stripped from the right, repeatedly,
    // but never past the second character: an app called "Free" stays "Free".
    static const QRegularExpression tail(
        QStringLiteral("\\s+(for (kids|children)|apps?|hd|lite|free)$"),
        QRegularExpression::CaseInsensitiveOption);
    while (true) {
        const qsizetype at = tail.match(name).capturedStart();
        if (at < 2)
            break;
        name.chop(name.size() - at);
    }

    name = name.simplified();
    if (name.isEmpty())
        return rawName.trimmed();

    // A name typed entirely in lower case is a shortcut, not a label. Anything already cased is
    // left exactly as it is, so "iPhone" and "Paint.NET" survive.
    bool anyUpper = false;
    for (const QChar c : name) {
        if (c.isUpper()) {
            anyUpper = true;
            break;
        }
    }
    if (!anyUpper) {
        bool wordStart = true;
        for (QChar &c : name) {
            if (wordStart && c.isLower())
                c = c.toUpper();
            wordStart = c == QLatin1Char(' ');
        }
    }

    return name;
}

QList<AppTile> parseStartMenuApps(const QString &output)
{
    QList<AppTile> apps;
    QSet<QString> seen;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab <= 0)
            continue;
        const QString name = QStringView(line).left(tab).trimmed().toString();
        const QString id = QStringView(line).mid(tab + 1).trimmed().toString();
        if (name.isEmpty() || id.isEmpty()
            || name.contains(QLatin1String("Uninstall"), Qt::CaseInsensitive))
            continue;

        AppTile tile;
        if (id.contains(QLatin1Char('!')))
            tile = AppTile{name, QStringLiteral("C:\\Windows\\explorer.exe"),
                           QStringLiteral("shell:AppsFolder\\%1").arg(id)};
        else if (QDir::isAbsolutePath(id) && id.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
            tile = AppTile{name, QDir::toNativeSeparators(id), {}};
        else
            continue; // ms-settings:, shell GUIDs, RegisteredApp names: nothing to launch

        if (!seen.contains(id)) {
            seen.insert(id);
            apps.append(tile);
        }
    }

    std::sort(apps.begin(), apps.end(), [](const AppTile &a, const AppTile &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return apps;
}

QList<AppTile> installedApps()
{
    const QString powershell = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"))
        + QStringLiteral("\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
    QProcess ps;
    ps.start(powershell,
             {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"),
              QStringLiteral("[Console]::OutputEncoding=[Text.Encoding]::UTF8; "
                             "Get-StartApps | ForEach-Object { $_.Name + [char]9 + $_.AppID }")});
    if (!ps.waitForFinished(15000))
        return {};
    return parseStartMenuApps(QString::fromUtf8(ps.readAllStandardOutput()));
}
