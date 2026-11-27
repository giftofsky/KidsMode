#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>

#include "policy.h"
#include "state.h"

class HudWindow;
class LauncherWindow;
class SessionTimer;

class SessionController : public QObject
{
    Q_OBJECT
public:
    SessionController(LauncherWindow *launcher, HudWindow *hud, QObject *parent = nullptr);

    bool start();

private:
    enum class State { Launcher, AppRunning, Locked, Settings };

    void enterLauncher();
    void enterLocked();
    bool showSetup();
    void showSettings();
    void requestApp(const AppTile &tile);
    void onChildFinished();
    void onChildError(QProcess::ProcessError error);
    void stopChild();
    void goHome();
    void onTick();
    void onPinSubmitted(const QString &pin);
    void addTime();
    bool suspendChild();
    void resumeChild();
    void save();
    void updateHud();
    int totalBudget() const;
    void killOrphan();
    void pollChildWindow();
    void enforceZOrder();
    void enforceChildOnTop();
    void enforceLauncherOnScreen();
    void bringChildToFront();
    void shieldAsDesktop();
    void setTaskbarVisible(bool visible);
    bool adoptLaunchedWindow();
    void finishApp();

    LauncherWindow *m_launcher;
    HudWindow *m_hud;
    SessionTimer *m_timer;
    QProcess m_child;
    QTimer m_clock;
    QElapsedTimer m_childTimer;
    QTimer m_windowPoll;
    QTimer m_guard;
    QSet<quintptr> m_windowsBefore;
    quintptr m_childWindow = 0; // HWND kept on top of the backdrop
    qint64 m_windowPid = 0;     // process that owns that window
    bool m_adoptedWindow = false;
    bool m_adopting = false;
    int m_windowTries = 0;
    Policy m_policy;
    UsageState m_usage;
    bool m_suspended = false; // the app was hidden, not killed, when the time ran out
    State m_state = State::Launcher;
};
