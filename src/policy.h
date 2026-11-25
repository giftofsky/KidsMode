#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

struct AppTile
{
    QString name;
    QString exePath;
    QString args;
};

// Written unelevated into AppData. The kid can edit it, and that is accepted:
// this is a family launcher, not parental control.
struct Policy
{
    int budgetSeconds = 3600;
    QByteArray pinSalt;
    QByteArray pinHash;
    int pinIterations = 100000;
    QList<AppTile> apps;

    bool hasPin() const { return !pinHash.isEmpty(); }

    static Policy load(const QString &path, bool *ok = nullptr);
    bool save(const QString &path) const;

    QJsonObject toJson() const;
    static Policy fromJson(const QJsonObject &object);

    static QString defaultPath();
};

// The Start Menu "All apps" list as tiles: desktop apps keep their exe, Store apps
// launch through explorer.exe + shell:AppsFolder, which the child-window adoption
// already handles. The split-out parser exists for the tests.
QList<AppTile> parseStartMenuApps(const QString &output);
QList<AppTile> installedApps();
