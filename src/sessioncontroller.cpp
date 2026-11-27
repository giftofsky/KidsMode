#include "sessioncontroller.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QStandardPaths>
#include <QVector>

#include "hudwindow.h"
#include "launcherwindow.h"
#include "logging.h"
#include "pin.h"
#include "sessiontimer.h"
#include "settingsdialog.h"

#include <algorithm>
#include <cwchar>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

void killIfSameImage(qint64 pid, const QString &exePath)
{
    if (pid <= 0 || exePath.isEmpty())
        return;

    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE,
                                static_cast<DWORD>(pid));
    if (!handle)
        return;

    wchar_t buffer[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    bool matches = QueryFullProcessImageNameW(handle, 0, buffer, &size) != FALSE;
    if (matches) {
        matches = QString::fromWCharArray(buffer, static_cast<int>(size))
                      .compare(exePath, Qt::CaseInsensitive)
            == 0;
    }
    if (matches)
        TerminateProcess(handle, 1);
    CloseHandle(handle);
}

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM param)
{
    auto *windows = reinterpret_cast<QSet<quintptr> *>(param);
    if (IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) > 0)
        windows->insert(reinterpret_cast<quintptr>(hwnd));
    return TRUE;
}

QSet<quintptr> visibleWindows()
{
    QSet<quintptr> windows;
    EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

BOOL CALLBACK findWindow(HWND hwnd, LPARAM param)
{
    auto *lookup = reinterpret_cast<QPair<DWORD, HWND> *>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != lookup->first || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)
        || GetWindowTextLengthW(hwnd) == 0)
        return TRUE;
    lookup->second = hwnd;
    return FALSE;
}

HWND mainWindowOf(qint64 pid)
{
    QPair<DWORD, HWND> lookup{static_cast<DWORD>(pid), nullptr};
    EnumWindows(findWindow, reinterpret_cast<LPARAM>(&lookup));
    return lookup.second;
}

// A window that may be the app a Store stub just handed off to.
bool adoptableWindow(HWND hwnd, const QSet<quintptr> &before)
{
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return false;
    if (before.contains(reinterpret_cast<quintptr>(hwnd)))
        return false;
    if (GetWindow(hwnd, GW_OWNER) || GetWindowTextLengthW(hwnd) == 0)
        return false;
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid != 0 && pid != GetCurrentProcessId();
}
// A Store app hands the launch to a stub that exits, and the window the child sees belongs to
// the frame host, not to the app: prefer that frame over the app's own content window.
struct AdoptLookup {
    const QSet<quintptr> *before;
    HWND frame = nullptr;
    HWND fallback = nullptr;
};

BOOL CALLBACK findAdoptableWindow(HWND hwnd, LPARAM param)
{
    auto *lookup = reinterpret_cast<AdoptLookup *>(param);
    if (!adoptableWindow(hwnd, *lookup->before))
        return TRUE;
    wchar_t className[64] = {};
    GetClassNameW(hwnd, className, 63);
    if (wcscmp(className, L"ApplicationFrameWindow") == 0) {
        lookup->frame = hwnd;
        return FALSE;
    }
    if (!lookup->fallback)
        lookup->fallback = hwnd;
    return TRUE; // keep looking for the frame
}

// The windows that must stay above the backdrop: the app's own and the one we adopted, in their
// current order, top of the z-order first. Minimised windows are left alone: the caller restores
// the app's window instead.
struct RaiseSet {
    DWORD pid;
    HWND main;
    QVector<HWND> windows;
};

BOOL CALLBACK collectRaiseWindows(HWND hwnd, LPARAM param)
{
    auto *set = reinterpret_cast<RaiseSet *>(param);
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (hwnd == set->main || (set->pid != 0 && pid == set->pid))
        set->windows.append(hwnd);
    return TRUE;
}

// Our own dialogs must sit above the topmost launcher and above the child app: the launcher
// turns topmost again the moment the app closes, which would bury a plain dialog under it.
void onTop(QWidget *dialog)
{
    dialog->setWindowFlag(Qt::WindowStaysOnTopHint, true);
}

void warn(QWidget *parent, const QString &title, const QString &text,
          QMessageBox::Icon icon = QMessageBox::Warning)
{
    QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
    onTop(&box);
    box.exec();
}

bool confirm(const QString &title, const QString &text)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    onTop(&box);
    return box.exec() == QMessageBox::Yes;
}

// A dialog can outlive the app it was opened over; the launcher turns topmost again at that
// point and would bury it.
void keepDialogOnTop()
{
    if (QWidget *dialog = QApplication::activeModalWidget()) {
        dialog->raise();
        dialog->activateWindow();
    }
}

} // namespace

SessionController::SessionController(LauncherWindow *launcher, HudWindow *hud, QObject *parent)
    : QObject(parent)
    , m_launcher(launcher)
    , m_hud(hud)
    , m_timer(new SessionTimer(SessionTimer::Clock(), this))
{
    connect(m_launcher, &LauncherWindow::appRequested, this, &SessionController::requestApp);
    connect(m_launcher, &LauncherWindow::parentRequested, this, &SessionController::showSettings);
    connect(m_launcher, &LauncherWindow::pinSubmitted, this, &SessionController::onPinSubmitted);
    connect(m_launcher, &LauncherWindow::addTimeRequested, this, &SessionController::addTime);
    connect(m_launcher, &LauncherWindow::quitRequested, this, [this] {
        qCInfo(kidsmode) << "quit requested";
        save();
        // Deferred, not a direct qApp->quit(): called while a click is still being dispatched
        // here the direct call is swallowed and the loop never returns (seen on this machine
        // with Qt 6.11; a plain test app quits fine). exit() from a timer fires reliably.
        QTimer::singleShot(0, qApp, [] { qApp->exit(0); });
    });
    connect(m_hud, &HudWindow::homeRequested, this, &SessionController::goHome);

    connect(m_timer, &SessionTimer::exhausted, this, &SessionController::enterLocked);
    connect(m_timer, &SessionTimer::warning, this, [](int) { QApplication::beep(); });
    connect(m_timer, &SessionTimer::dayRolledOver, this, [this] {
        m_usage.bonusSeconds = 0;
        m_timer->setBudgetSeconds(totalBudget());
        save();
    });

    connect(&m_child, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        qCInfo(kidsmode) << "child finished, code" << code << "status" << int(status) << "after"
                         << m_childTimer.elapsed() << "ms";
        onChildFinished();
    });
    connect(&m_child, &QProcess::errorOccurred, this, &SessionController::onChildError);
    connect(&m_child, &QProcess::started, this, [this] {
        m_usage.childPid = m_child.processId();
        m_usage.childExe = m_child.program();
        m_childTimer.start();
        m_windowTries = 0;
        m_windowPoll.start();
        // We were the foreground window just before hiding, so let the app take focus.
        AllowSetForegroundWindow(static_cast<DWORD>(m_usage.childPid));
        qCInfo(kidsmode) << "child started, pid" << m_usage.childPid << m_usage.childExe;
        save();
    });

    m_clock.setInterval(1000);
    connect(&m_clock, &QTimer::timeout, this, &SessionController::onTick);

    m_windowPoll.setInterval(250);
    connect(&m_windowPoll, &QTimer::timeout, this, &SessionController::pollChildWindow);
    m_guard.setInterval(1000);
    connect(&m_guard, &QTimer::timeout, this, &SessionController::enforceZOrder);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        qCInfo(kidsmode) << "about to quit";
        setTaskbarVisible(true);
        save();
        stopChild();
    });
}

bool SessionController::start()
{
    bool ok = false;
    m_policy = Policy::load(Policy::defaultPath(), &ok);
    qCInfo(kidsmode) << "policy loaded, ok" << ok << "path" << Policy::defaultPath() << "budget"
                     << m_policy.budgetSeconds << "apps" << m_policy.apps.size();
    if (!ok || !m_policy.hasPin() || m_policy.apps.isEmpty()) {
        if (!showSetup())
            return false;
    }

    m_usage = UsageState::load(UsageState::defaultPath());
    m_usage.rollToDate(QDate::currentDate());
    qCInfo(kidsmode) << "usage loaded, date" << m_usage.date << "secondsUsed" << m_usage.secondsUsed
                     << "bonus" << m_usage.bonusSeconds;

    m_timer->setBudgetSeconds(totalBudget());
    m_timer->restore(m_usage.secondsUsed, m_usage.date);
    killOrphan();

    m_launcher->setApps(m_policy.apps);
    m_clock.start();
    setTaskbarVisible(false);

    if (m_timer->isExhausted())
        enterLocked();
    else
        enterLauncher();
    m_guard.start(); // keeps our windows on top for the whole session
    return true;
}

bool SessionController::showSetup()
{
    SettingsDialog dialog(m_policy, true);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    m_policy = dialog.policy();
    if (!m_policy.save(Policy::defaultPath())) {
        warn(nullptr, tr("Kids Mode"), tr("Could not save the settings."), QMessageBox::Critical);
        return false;
    }
    return true;
}

void SessionController::enterLauncher()
{
    if (m_timer->isExhausted()) {
        enterLocked();
        return;
    }

    m_state = State::Launcher;
    m_windowPoll.stop();
    m_childWindow = 0;
    m_windowPid = 0;
    m_adoptedWindow = false;
    m_hud->hide();
    m_timer->setRunning(true);
    m_launcher->setRemaining(m_timer->remainingSeconds(), totalBudget());
    m_launcher->showTiles();
    m_launcher->setTopmost(true);
    m_launcher->showFullScreen();
    m_launcher->setAssertFocus(true);
    keepDialogOnTop();
    qCInfo(kidsmode) << "state Launcher, remaining" << m_timer->remainingSeconds() << "of"
                     << totalBudget();
    save();
}

void SessionController::enterLocked()
{
    // The child was playing when the time ran out, so remember what: "+15 minutes" hands the
    // same app back instead of sending them hunting for the tile again.
    m_childInterrupted = m_state == State::AppRunning;
    if (m_childInterrupted)
        qCInfo(kidsmode) << "time ran out while running" << m_childTile.name;

    m_state = State::Locked;
    m_timer->setRunning(false);
    stopChild();
    m_childWindow = 0;
    m_windowPid = 0;
    m_adoptedWindow = false;
    save();

    m_hud->hide();
    m_launcher->showLock();
    m_launcher->setTopmost(true);
    m_launcher->showFullScreen();
    m_launcher->setAssertFocus(true);
    keepDialogOnTop();
    qCInfo(kidsmode) << "state Locked, secondsUsed" << m_timer->secondsUsed() << "budget"
                     << totalBudget();
}

void SessionController::requestApp(const AppTile &tile)
{
    if (m_state != State::Launcher)
        return;

    QStringList args = QProcess::splitCommand(tile.args);

    // Chromium hands the command to an already-running browser and exits, which looks
    // like "the app closed" and brings the launcher back over it. A private profile
    // forces a browser process Kids Mode actually owns.
    const QString fileName = QFileInfo(tile.exePath).fileName().toLower();
    if (fileName == QStringLiteral("chrome.exe") || fileName == QStringLiteral("msedge.exe")) {
        const bool hasProfile = std::any_of(args.cbegin(), args.cend(), [](const QString &arg) {
            return arg.startsWith(QStringLiteral("--user-data-dir"), Qt::CaseInsensitive);
        });
        if (!hasProfile) {
            const QString profile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/browser-profile");
            QDir().mkpath(profile);
            args << QStringLiteral("--user-data-dir=") + QDir::toNativeSeparators(profile)
                 << QStringLiteral("--no-first-run")
                 << QStringLiteral("--no-default-browser-check");
        }
    }

    m_childTile = tile;
    m_state = State::AppRunning;
    m_hud->setRemaining(m_timer->remainingSeconds(), totalBudget());
    m_launcher->setAssertFocus(false);
    m_launcher->showShield(); // cream first, so the z-order change below has nothing to expose
    m_launcher->setTopmost(false);
    m_launcher->showFullScreen();
    shieldAsDesktop();
    m_windowsBefore = visibleWindows();
    m_adoptedWindow = false;
    m_childWindow = 0;
    m_windowPid = 0;

    m_child.setProgram(tile.exePath);
    m_child.setArguments(args);
    m_child.setWorkingDirectory(QFileInfo(tile.exePath).absolutePath());
    m_child.start();

    m_hud->show();
    qCInfo(kidsmode) << "launch" << tile.name << tile.exePath << "args" << args
                     << "launcherVisible" << m_launcher->isVisible();
    save();
}

void SessionController::onTick()
{
    if (m_state != State::Launcher && m_state != State::AppRunning)
        return;

    m_timer->tick();

    if (m_state == State::Launcher)
        m_launcher->setRemaining(m_timer->remainingSeconds(), totalBudget());
    else if (m_state == State::AppRunning)
        updateHud();

    if (m_timer->secondsUsed() % 10 == 0)
        save();
}

void SessionController::updateHud()
{
    m_hud->setRemaining(m_timer->remainingSeconds(), totalBudget());
}

void SessionController::onChildFinished()
{
    if (m_state != State::AppRunning)
        return;

    if (m_adoptedWindow)
        return; // the guard watches that window

    // A Store app (calc.exe, mspaint.exe) is a stub: it exits at once and the real window
    // belongs to another process. Wait for that window and take it over.
    // shortcut: only a window that appeared after the launch is adopted; if none shows up
    // within 10 s the launcher comes back. 10 s because Store games open slower than Calc.
    if (m_childWindow == 0 && m_childTimer.isValid() && m_childTimer.elapsed() < 10000) {
        m_adopting = true;
        m_windowTries = 0;
        m_windowPoll.start();
        qCInfo(kidsmode) << "child exited without a window, looking for one to adopt";
        return;
    }

    finishApp();
}

void SessionController::finishApp()
{
    m_windowPoll.stop();
    m_adopting = false;
    m_adoptedWindow = false;
    m_childWindow = 0;
    m_windowPid = 0;
    m_usage.childPid = 0;
    m_usage.childExe.clear();
    enterLauncher();
}

void SessionController::onChildError(QProcess::ProcessError error)
{
    qCWarning(kidsmode) << "child error" << int(error) << m_child.errorString();

    if (error != QProcess::FailedToStart || m_state != State::AppRunning)
        return;

    warn(nullptr, tr("Kids Mode"), tr("This app could not be started."));
    enterLauncher();
}

void SessionController::stopChild()
{
    m_windowPoll.stop();

    if (m_adoptedWindow) {
        auto *window = reinterpret_cast<HWND>(m_childWindow);
        if (window && IsWindow(window)) {
            PostMessageW(window, WM_CLOSE, 0, 0);
            for (int i = 0; i < 50 && IsWindow(window); ++i)
                Sleep(100);
        }
        qCInfo(kidsmode) << "stopChild: adopted window"
                         << (!window || !IsWindow(window) ? "closed" : "still open");
        m_adoptedWindow = false;
        m_childWindow = 0;
        m_windowPid = 0;
        m_usage.childPid = 0;
        m_usage.childExe.clear();
        return;
    }

    if (m_child.state() == QProcess::NotRunning) {
        qCInfo(kidsmode) << "stopChild: nothing running";
        m_childWindow = 0;
        m_windowPid = 0;
        return;
    }

    m_child.terminate();
    const bool closed = m_child.waitForFinished(5000);
    if (!closed)
        m_child.kill();
    if (m_child.state() != QProcess::NotRunning)
        m_child.waitForFinished(2000);

    qCInfo(kidsmode) << "stopChild pid" << m_child.processId() << "graceful" << closed;
    m_childWindow = 0;
    m_windowPid = 0;
    m_usage.childPid = 0;
    m_usage.childExe.clear();
}

void SessionController::pollChildWindow()
{
    if (m_state != State::AppRunning) {
        m_windowPoll.stop();
        return;
    }

    if (m_adopting) {
        if (adoptLaunchedWindow()) {
            m_adopting = false;
            m_windowPoll.stop();
            return;
        }
        if (++m_windowTries > 20) {
            qCInfo(kidsmode) << "no window appeared to adopt";
            finishApp();
        }
        return;
    }

    if (m_usage.childPid <= 0) {
        m_windowPoll.stop();
        return;
    }

    if (HWND window = mainWindowOf(m_usage.childPid)) {
        m_childWindow = reinterpret_cast<quintptr>(window);
        m_windowPid = m_usage.childPid;
        m_windowPoll.stop();
        qCInfo(kidsmode) << "child window found, pid" << m_windowPid;
        bringChildToFront();
        return;
    }

    if (++m_windowTries > 40)
        m_windowPoll.stop();
}

void SessionController::bringChildToFront()
{
    if (m_state != State::AppRunning)
        return;

    // The launcher stays on screen as the wallpaper while an app runs.
    if (!m_launcher->isVisible() || m_launcher->isMinimized()) {
        m_launcher->showShield();
        m_launcher->showFullScreen();
    }
    shieldAsDesktop();

    // Everything the app shows goes above the backdrop, bottom-most first, so the app's own
    // order - a dialog above its window - survives and the window it was working in stays on top.
    RaiseSet set{static_cast<DWORD>(m_usage.childPid), reinterpret_cast<HWND>(m_childWindow), {}};
    EnumWindows(collectRaiseWindows, reinterpret_cast<LPARAM>(&set));
    for (auto it = set.windows.crbegin(); it != set.windows.crend(); ++it)
        SetWindowPos(*it, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    HWND top = set.windows.isEmpty() ? set.main : set.windows.first();
    if (!top || !IsWindow(top)) {
        m_hud->raise();
        return;
    }
    if (IsIconic(top)) // the child pressed the app's minimise button
        ShowWindow(top, SW_RESTORE);

    // Only take focus when the app does not already have it: the child must be able to type.
    bool appHasFocus = false;
    if (HWND foreground = GetForegroundWindow()) {
        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(foreground, &foregroundPid);
        appHasFocus = set.windows.contains(foreground)
            || (set.pid != 0 && foregroundPid == set.pid)
            || foregroundPid == static_cast<DWORD>(m_windowPid);
    }
    if (!appHasFocus)
        SetForegroundWindow(top);
    m_hud->raise();
}

// The launcher window is the child's desktop: above every other window, below the app and below
// our own dialogs. Anything else left on screen is visible and clickable to a child.
void SessionController::shieldAsDesktop()
{
    auto *shield = reinterpret_cast<HWND>(m_launcher->winId());
    if (IsIconic(shield))
        ShowWindow(shield, SW_RESTORE);
    SetWindowPos(shield, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// The taskbar holds Start and an icon for every other app, so it is the exit we cannot show.
// shortcut: a crash leaves it hidden until the next Kids Mode start or an explorer restart.
void SessionController::setTaskbarVisible(bool visible)
{
    if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr))
        ShowWindow(tray, visible ? SW_SHOW : SW_HIDE);
}

// Runs every second for the whole session: whatever the child does to the windows, our screen
// ends up on top of everything else.
void SessionController::enforceZOrder()
{
    switch (m_state) {
    case State::AppRunning:
        enforceChildOnTop();
        break;
    case State::Launcher:
    case State::Locked:
        enforceLauncherOnScreen();
        break;
    case State::Settings:
        break; // our own dialogs own this state
    }
}

void SessionController::enforceLauncherOnScreen()
{
    if (QApplication::activeModalWidget())
        return;

    if (m_launcher->isMinimized())
        m_launcher->showFullScreen(); // Win+D, "Show desktop"
    m_launcher->setTopmost(true); // raises it, and re-asserts the style Qt no longer holds for us
}

void SessionController::enforceChildOnTop()
{
    if (m_state != State::AppRunning)
        return;

    if (m_adoptedWindow
        && (!m_childWindow || !IsWindow(reinterpret_cast<HWND>(m_childWindow)))) {
        qCInfo(kidsmode) << "adopted window closed";
        finishApp();
        return;
    }

    // A dialog of ours (Home, PIN) is topmost and must keep the focus.
    HWND foreground = GetForegroundWindow();
    DWORD pid = 0;
    if (foreground)
        GetWindowThreadProcessId(foreground, &pid);
    if (pid == GetCurrentProcessId()
        && foreground != reinterpret_cast<HWND>(m_launcher->winId()))
        return;

    // Re-asserted every tick rather than on a foreground change: a window can also be lifted
    // without becoming the foreground window, and then nothing else would put it back.
    bringChildToFront();
}

bool SessionController::adoptLaunchedWindow()
{
    AdoptLookup lookup{&m_windowsBefore};
    EnumWindows(findAdoptableWindow, reinterpret_cast<LPARAM>(&lookup));
    HWND window = lookup.frame ? lookup.frame : lookup.fallback;
    if (!window)
        return false;

    // The frame, not the UWP content window, is the window we then own.
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);

    m_adoptedWindow = true;
    m_windowPid = pid;
    m_childWindow = reinterpret_cast<quintptr>(window);
    qCInfo(kidsmode) << "adopted window" << m_childWindow << "of pid" << pid;
    bringChildToFront();
    return true;
}

void SessionController::goHome()
{
    if (m_state != State::AppRunning)
        return;

    // The guard lets our own dialogs through, so the question lands above the app.
    if (!confirm(tr("Kids Mode"), tr("Finish playing and go back?")))
        return;

    m_state = State::Launcher; // ignore the finished() signal from the kill below
    stopChild();
    enterLauncher();
}

void SessionController::onPinSubmitted(const QString &pin)
{
    if (m_state != State::Locked)
        return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_usage.isLockedOutAt(now)) {
        const int minutes = static_cast<int>((m_usage.failedPinUntilEpoch - now + 59) / 60);
        m_launcher->showPinError(tr("Too many tries. Wait %1 min.").arg(minutes));
        return;
    }

    if (Pin::verify(pin, m_policy.pinSalt, m_policy.pinHash, m_policy.pinIterations)) {
        m_usage.failedPins = 0;
        m_usage.failedPinUntilEpoch = 0;
        save();
        m_launcher->showParentChoices();
        return;
    }

    ++m_usage.failedPins;
    const int lockout = Pin::lockoutSecondsAfterFailure(m_usage.failedPins);
    if (lockout > 0)
        m_usage.failedPinUntilEpoch = now + lockout;
    save();

    m_launcher->showPinError(lockout > 0
                                 ? tr("Too many tries. Wait %1 min.").arg(lockout / 60)
                                 : tr("Wrong PIN. Try again."));
}

void SessionController::addTime()
{
    if (m_state != State::Locked)
        return;

    m_usage.bonusSeconds += 15 * 60;
    m_timer->setBudgetSeconds(totalBudget());
    save();
    enterLauncher();

    // They asked for more time for the app that just ran out, so it comes back by itself.
    if (m_childInterrupted) {
        m_childInterrupted = false;
        if (QFileInfo::exists(m_childTile.exePath)) {
            qCInfo(kidsmode) << "relaunching after +15 min" << m_childTile.name;
            requestApp(m_childTile);
        }
    }
}

void SessionController::showSettings()
{
    if (m_state != State::Launcher)
        return;

    m_state = State::Settings;
    m_timer->setRunning(false);
    m_launcher->setAssertFocus(false);
    m_launcher->showShield(); // cream first, so the z-order change below has nothing to expose
    m_launcher->setTopmost(false);
    m_launcher->showFullScreen();

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_usage.isLockedOutAt(now)) {
        enterLauncher();
        return;
    }

    QInputDialog pinBox;
    pinBox.setWindowTitle(tr("Parent"));
    pinBox.setLabelText(tr("PIN:"));
    pinBox.setTextEchoMode(QLineEdit::Password);
    onTop(&pinBox);
    if (pinBox.exec() != QDialog::Accepted) {
        enterLauncher();
        return;
    }
    const QString pin = pinBox.textValue();

    if (!Pin::verify(pin, m_policy.pinSalt, m_policy.pinHash, m_policy.pinIterations)) {
        ++m_usage.failedPins;
        const int lockout = Pin::lockoutSecondsAfterFailure(m_usage.failedPins);
        if (lockout > 0)
            m_usage.failedPinUntilEpoch = now + lockout;
        save();
        warn(nullptr, tr("Parent"), tr("Wrong PIN."));
        enterLauncher();
        return;
    }

    m_usage.failedPins = 0;
    m_usage.failedPinUntilEpoch = 0;
    save();

    QMessageBox choice(QMessageBox::Question, tr("Parent"), tr("What would you like to do?"),
                       QMessageBox::NoButton, nullptr);
    QPushButton *settings = choice.addButton(tr("Settings..."), QMessageBox::AcceptRole);
    QPushButton *quit = choice.addButton(tr("Quit Kids Mode"), QMessageBox::DestructiveRole);
    choice.addButton(QMessageBox::Cancel);
    onTop(&choice);
    choice.exec();
    qCInfo(kidsmode) << "parent choice"
                     << (choice.clickedButton() == quit        ? "quit"
                         : choice.clickedButton() == settings  ? "settings"
                                                               : "cancel");

    if (choice.clickedButton() == quit) {
        save();
        QTimer::singleShot(0, qApp, [] { qApp->exit(0); }); // deferred, see quitRequested
        return;
    }
    if (choice.clickedButton() != settings) {
        enterLauncher();
        return;
    }

    // Not onTop: this dialog owns the native file picker, which would land behind it.
    SettingsDialog dialog(m_policy, false);
    if (dialog.exec() == QDialog::Accepted) {
        m_policy = dialog.policy();
        if (!m_policy.save(Policy::defaultPath()))
            warn(nullptr, tr("Kids Mode"), tr("Could not save the settings."));
        m_launcher->setApps(m_policy.apps);
        m_timer->setBudgetSeconds(totalBudget());
    }
    enterLauncher();
}

void SessionController::save()
{
    m_usage.date = m_timer->date();
    m_usage.secondsUsed = m_timer->secondsUsed();
    if (!m_usage.save(UsageState::defaultPath()))
        qWarning("KidsMode: could not save state");
}

int SessionController::totalBudget() const
{
    return qMax(0, m_policy.budgetSeconds + m_usage.bonusSeconds);
}

void SessionController::killOrphan()
{
    if (m_usage.childPid > 0)
        killIfSameImage(m_usage.childPid, m_usage.childExe);
    m_usage.childPid = 0;
    m_usage.childExe.clear();
}
