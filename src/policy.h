#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// A short label a child can read, from the raw Start Menu name: "Math Kids: Math Games For Kids"
// becomes "Math Kids". The store-page tail and the "for kids/Apps" boilerplate say nothing to a
// six-year-old. Anything the heuristic gets wrong a parent can rename in Settings.
QString friendlyName(const QString &rawName);

struct AppTile
{
    QString name;
    QString exePath;
    QString args;
    // The label the child sees. Empty means "use friendlyName(name)"; a parent can set anything.
    QString shownName;

    QString label() const { return shownName.isEmpty() ? friendlyName(name) : shownName; }
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
