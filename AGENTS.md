# AGENTS.md

Windows 11 kids launcher: a full-screen grid of parent-approved apps with a daily time budget.
When the time runs out it closes the running app and locks the screen until a parent types the
PIN. Then the parent can **add 15 minutes** or **quit Kids Mode**.

**Status:** `KidsModeCore` (timer, policy, state, pin), `SessionController` and the Widgets UI
build; the QTest suites pass. Git repo initialized, no remote yet.

**Simple by design.** This is a family tool, not parental-control software. It is a UI boundary,
not a security boundary: Ctrl+Alt+Del, Task Manager, another account or a reboot can get around
it, and that is accepted. Do not add elevation, ACLs, services, registry policy, a keyboard hook
or shell replacement. Say the same sentence in the README and the settings dialog.

**Stack:** Qt 6 Widgets, C++20, CMake. No third-party libraries.

## What it does

- Full-screen launcher, one child app at a time, Win32 `.exe` only. No `.lnk`. A Store stub
  (`calc.exe`, Windows 11's `notepad.exe`) is followed to the window it opens, see Process handling.
- Counts seconds only while the launcher or the child app is on screen, against a daily budget.
  The count is saved, so a restart or reboot does not refill the day.
- At zero: terminate the child (5 s grace), kill it, save, then show the lock screen.
- While an app runs the launcher stays behind it as a blank cream backdrop, so the desktop is
  never visible around a small window.
- Lock screen: "Time's up!", a 4-dot PIN display and a 3x4 keypad. Correct PIN → two buttons:
  **Add 15 minutes** / **Quit Kids Mode**.
- The launcher has a small **Parent** button (and a 2 s press on the time bar). PIN → three
  choices: **Settings…** / **Quit Kids Mode** / Cancel. Without it there is no way out.
- Warnings at 5 and 1 minute, amber. Never pure red.
- Everything the app does is logged to `kidsmode.log`. No telemetry.
- Single child account, primary screen only, no network.

## Wiring

`main.cpp` builds the objects. `SessionController` owns the state machine, the timer and the
`QProcess`. Windows draw and emit signals; they never touch `QProcess`, files or `Policy`.

| Class | Owns |
|---|---|
| `main.cpp` | application name, logger, single-instance `QLockFile`, constructs the three objects |
| `SessionController` | states, `Policy`, `UsageState`, `SessionTimer`, `QProcess`, the child window handle and its 1 s guard, saves |
| `LauncherWindow` | tiles, time bar, lock keypad; emits `appRequested`, `parentRequested`, `pinSubmitted`, `addTimeRequested`, `quitRequested` |
| `HudWindow` | remaining time + Home; emits `homeRequested` |
| `SettingsDialog` | PIN, budget, app list; returns a `Policy` |

## State machine

| State | Visible | Child | Timer |
|---|---|---|---|
| `Launcher` | full screen, topmost | none | running |
| `AppRunning` | cream backdrop (launcher, bottom of the z-order) + HUD strip | exactly 1 | running |
| `Locked` | full screen, topmost | none | paused |
| `Settings` | modal dialog over the cream backdrop | none | paused |

- start → `Settings` (setup) when `policy.json` is missing, has no PIN or has no apps;
  cancelled → quit.
- start → `Locked` when today's budget is spent, else `Launcher`.
- `Launcher` + tile → clear topmost, show the blank cream shield page, *then* start the process.
- child `finished()` → `Launcher` (which locks instead if the budget just ran out).
- HUD Home → confirm, stop the child → `Launcher`.
- exhausted → stop the child → save → `Locked`.
- `Locked` + correct PIN → parent choices → add 15 min (`Launcher`) or quit.
- `Launcher`: the **Parent** button, or a 2 s press on the time bar → PIN → Settings… / Quit / Cancel.

Rules:

- **The launcher is never hidden.** While an app runs it shows a blank cream page and acts as the
  fake desktop: `shieldAsDesktop()` puts it at `HWND_TOP` of the non-topmost band, then every
  window of the child app is raised above it (bottom-most first, so the app's own dialogs keep
  their order). Nothing between the two is ever visible.
- **The taskbar is hidden for the whole session** (`setTaskbarVisible(false)` in `start()`,
  restored in `aboutToQuit`). `shortcut: a hard kill leaves it hidden; a reboot fixes it.`
- **The launcher ignores `closeEvent`** — Alt+F4 is not a way out.
- **Topmost is a state, not a window property.** Only `Launcher`, `Locked` and the HUD are
  topmost. A topmost child window would hide its own file dialogs, so the app is *not* made
  topmost; it is raised with `HWND_TOP` and `SetForegroundWindow` instead.
- **The launcher flips topmost with Win32, not Qt** (`LauncherWindow::setTopmost` →
  `SetWindowPos(HWND_TOPMOST | HWND_NOTOPMOST)`). Qt's flags are set once in the constructor:
  `setWindowFlag()` on the visible launcher hides and re-creates the window, and the real desktop
  shows for a frame — that was the flicker in backlog 004. Because Qt no longer owns the style,
  `showEvent()` and the 1 s guard re-assert it.
- **Our dialogs are topmost** (`onTop()`), and `keepDialogOnTop()` re-raises one that outlived the
  app it was opened over — otherwise the launcher turning topmost again buries it and the child is
  stuck with no way home. The exception is `SettingsDialog`: topmost would push its native file
  picker behind it.
- The launcher does **not** re-assert focus while `QApplication::activeModalWidget()` is set, for
  the same reason.
- **`setWindowFlag()` hides a shown widget.** Always `showFullScreen()` after changing flags.
- **Quit is deferred**: `save()` then `QTimer::singleShot(0, qApp, [] { qApp->exit(0); })`. A direct
  `qApp->quit()` inside the click handler is swallowed on this machine (Qt 6.11) — the loop never
  returns and the taskbar stays hidden. Both quit paths (lock screen and Parent choice) do this.
- **The 1 s guard (`enforceZOrder`) runs for the whole session**, not just while an app runs: it
  re-raises the launcher in `Launcher`/`Locked` (Win+D, Alt+Tab, "Show desktop") and the backdrop +
  app in `AppRunning`. A window can be lifted without becoming foreground, so re-assert every tick.
- **Store apps**: adopt the `ApplicationFrameWindow` frame (owned by ApplicationFrameHost), not the
  app's own content window — that frame is what the child sees, what stays on top and what
  `WM_CLOSE` closes.
- **Kill the child before showing the lock screen** — exclusive-fullscreen apps stay above us.
- Re-assert focus only in `Launcher`/`Locked` on `WindowDeactivate`: `raise()` +
  `activateWindow()` after a 200 ms single-shot. Never while an app runs.
- A 1 s guard (`enforceZOrder` → `enforceChildOnTop` in `AppRunning`) puts the app back in front
  every tick, and restores it if it was minimized.

## Time budget

- Today's allowance is `budgetSeconds + bonusSeconds`; `bonusSeconds` holds the 15-minute
  top-ups and resets at midnight.
- `SessionTimer` takes an injected clock and a manual `tick()`. Tests never use wall time.
- Save on every state change, every 10 ticks, and on `aboutToQuit`.
  `shortcut: a hard power-off can lose the last few seconds; acceptable.`
- Warnings at 5 and 1 minute, re-armed when the parent adds time.
- `budgetSeconds == 0` is no time, not unlimited. The spin box floor is 5 minutes.

## Files

Both in `QStandardPaths::AppDataLocation` (`%APPDATA%\KidsMode\`); the app writes them
unelevated:

| File | Holds |
|---|---|
| `policy.json` | PBKDF2 salt + hash + iterations, `budgetSeconds`, apps (`exePath`, `args`, Windows `name`, optional `shownName`) |
| `state.json` | `date`, `secondsUsed`, `bonusSeconds`, `failedPins`, `failedPinUntilEpoch`, `childPid`, `childExe` |
| `kidsmode.log` | every state change, launch and child event; rotates to `.1` at 1 MB |
| `browser-profile/` | the profile Chrome/Edge start with, so we own their process |

`shortcut: the kid can edit or delete these files to extend the day. Accepted — a kid who does
that has outgrown the product; the README says so.`

- Missing `state.json` means zero used. Date changed → zero used, zero bonus.
- PIN: PBKDF2-SHA256, 16-byte random salt, 100k iterations, exactly 4 digits.
  `QPasswordDigestor` lives in **QtNetwork**, not QtCore — link `Qt6::Network`.
- Brute force: after 5 failures block for 5 minutes, doubling to a 1-hour cap.
- Save with `QSaveFile` so a crash cannot leave a truncated file.

## Process handling

- `QProcess::start(exePath, args)`, never through a shell. Keep the process object; never kill
  by image name — that kills the parent's copy of the same app too.
- Chromium (`chrome.exe`, `msedge.exe`): a normal launch hands the URL to an already-running
  browser and exits, so Kids Mode sees the app close at once and the fullscreen launcher comes
  back over it. Give it `--user-data-dir=<AppData>/browser-profile` (plus `--no-first-run`,
  `--no-default-browser-check`) so the process we start is the browser we own. The child has no
  access to the parent's Chrome profile, bookmarks or saved passwords — intended.
- `terminate()` posts `WM_CLOSE`; after 5 s `kill()`. Games ignore the first.
- A Store app is a stub: `calc.exe` exits in ~500 ms and the window belongs to
  `ApplicationFrameHost`; Windows 11's `notepad.exe` exits in ~200 ms and the window belongs to
  another `Notepad.exe`. The process `finished()` in under 5 s without a window is therefore not
  the end: `pollChildWindow()` adopts the first window that was not on screen before the launch
  (`m_windowsBefore`), and closing time posts `WM_CLOSE` to that window instead.
  `shortcut: only a window that appears within 10 s is adopted; otherwise the launcher comes back.`
- Store apps added from the Start Menu are stored as `explorer.exe` + `shell:AppsFolder\<AUMID>`;
  explorer exits at once and the game's `ApplicationFrameWindow` is adopted like any Store stub.
  `installedApps()` (in `policy.*`) runs `Get-StartApps` once to build the picker list.
- Apps that spawn their own children can leave orphans. Accepted for v1; the fix is a job object
  with `KILL_ON_JOB_CLOSE`, do not half-implement it.
- Record the child PID in `state.json`; on start, kill it only if the PID is alive **and** its
  image path matches the recorded exe.
- Single instance: `QLockFile` in the temp dir, 30 s stale time.

## UI

**Kid screen:** warm cream background, big tiles (icon 112 px, button 212x204, a pastel tint
picked by a hash of the tile's name so it keeps its colour across reorders), the kid pages in
`'Segoe UI Rounded','Comic Sans MS','Segoe UI'`, a bear beside "What shall we play?", a rounded
time bar with "N min left" centred inside it (amber under 5 minutes), and a small
low-contrast "🔒 **Parent**" button beside it. Glyphs live in their own labels so they stay out of
the a11y names ("Kids Mode", "N minutes left", "Parent"). Tile labels are
`friendlyName()` or the parent's `shownName` (Settings → **Rename…**), wrapped onto two lines by
`wrapName()`; the tooltip keeps the real Windows name and path. A tile whose exe is gone is
disabled with a tooltip, never crashing the paint. Tab + Enter must work;
visible focus ring. No badges, streaks or red.

**Lock screen:** same warm palette, "Time's up!", "Ask a grown-up to unlock.", four PIN dots and
a 3x4 keypad — a touchscreen with no keyboard is enough. A wrong PIN clears the dots and says so;
a lockout says how many minutes. The correct PIN swaps to two big buttons: *Add 15 minutes* and
*Quit Kids Mode*.

**HUD:** 230x44 strip, bottom-centre: remaining minutes + Home, amber under 5 minutes.
Faded to 55% opacity and woken to full by a mouse-enter or a press, fading back after 4 s idle —
faded, never hidden: a touchscreen has no hover and this is the only way home, and the amber
warning never fades.
`Qt::Tool | FramelessWindowHint | WindowStaysOnTopHint | WindowDoesNotAcceptFocus` plus
`WA_ShowWithoutActivating`. It is the only way back from a running app, so it is not optional.

**Settings:** neutral, dense, plain form. Budget spin box (5..480 min), app list with icons,
**Add from Start Menu…** (a searchable, checkable list of the Start Menu "All apps" — desktop
apps and Store games both; `installedApps()` in `policy.*`), Add (`*.exe`) / **Rename…** (sets
`shownName`, pre-filled with `friendlyName()`) / Remove (confirm),
PIN + confirm in setup, blank PIN in settings keeps the old one. The dialog keeps the plain system
font — it is for the parent, not the child.
One honest line: "Kids Mode is a boundary, not a lock: an administrator can always remove it."

## Layout

```
src/main.cpp              entry, application name, logger, single instance
src/sessioncontroller.*   states, timer, QProcess, save
src/launcherwindow.*      tiles, time bar, parent entry, lock keypad
src/hudwindow.*           running-app strip
src/settingsdialog.*      PIN, budget, apps
src/logging.*             qCInfo/qCWarning → kidsmode.log
src/policy.*              policy.json + AppTile
src/appicon.*             tile icons: shell jumbo logo for Store apps, exe icon otherwise
src/state.*               state.json
src/sessiontimer.*        injected clock, tick accounting
src/pin.*                 PBKDF2, lockout schedule
tests/                    QTest: timer, policy/state, pin
```

## Build

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=E:/Qt/6.11.0/msvc2022_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
E:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe build\Release\KidsMode.exe
```

- `find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Widgets Test)`.
- `KidsModeCore` is a static lib (no Widgets) so the tests stay headless. The `KidsMode` target
  is `qt_add_executable(... WIN32 ...)` — without `WIN32` every launch flashes a console.
- `QPasswordDigestor` is in QtNetwork, so `KidsModeCore` links `Qt6::Network`. Nothing in the app
  needs networking.
- Windows headers: `WIN32_LEAN_AND_MEAN` + `NOMINMAX`, included **after** all Qt headers and only
  in `.cpp` files. `sessioncontroller.cpp` (orphan cleanup) and `appicon.cpp` (shell icon lookup,
  which is why `KidsMode` links `shell32` + `comctl32`) need them.
- Do not use Ninja from a plain shell on this machine — MSVC's environment is not loaded. The
  Visual Studio generator does not need `vcvars`.
- From a plain shell `cmake`/`ctest` are not on PATH — use `E:\Qt\Tools\CMake_64\bin\`, and put
  `E:\Qt\6.11.0\msvc2022_64\bin` in PATH when running ctest, or every test fails with
  `0xc0000135` (the Qt DLLs are not next to the test exes).
- Kill `KidsMode.exe` before building, or the link step dies with LNK1104 (running exe locked).
- `jom` can skip dependents of a changed header: `hudwindow.h` gained a `QTimer` member,
  `main.cpp` was not rebuilt, and the app then crashed with an access violation in `Qt6Core`
  during startup (old object size, new constructor). After a header change that grows a class,
  touch the `.cpp` files that use it — or clean-build — before believing any crash you see.
- Qt Creator has the project open with the **Desktop Qt 6.11.0 MSVC2022 64bit** kit and a
  Release config; builds land in `build/Desktop_Qt_6_11_0_MSVC2022_64bit_Release`. Prefer the
  MCP tools so the kit stays consistent.
- Installer: `powershell -File installer\build-installer.ps1` → `build\KidsMode-Setup.exe` —
  one offline exe (IFW = Qt Installer Framework, `E:\Qt\Tools\QtInstallerFramework\4.10`),
  per-user install to `AppData\Local\Programs`,
  Start Menu + Startup shortcuts, ships the Qt DLLs and the MSVC CRT so it works on any
  Windows 10/11 x64 PC without admin. `installer/` holds the IFW config; the payload in
  `packages/com.kidsmode.app/data/` is generated — do not edit or commit it.
  `shortcut: IFW 4.10 quirks — no <AdminRequiredFor*> in config.xml (per-user target dir makes it
  moot); the component script must declare function Component() {} before overriding
  createOperations; <WizardStyle>Modern</WizardStyle> is required — the default wizard style paints
  the title-bar band dark with black title text in Windows dark mode (Qt renders it, QSS cannot
  reach it).`
- Test the installer headless: `KidsMode-Setup.exe install com.kidsmode.app --root <dir> --am
  --confirm-command`, then `maintenancetool.exe purge --root <dir> --am --confirm-command`.

## Testing

- `SessionTimer`: counts only while running, exhaustion exactly at the boundary once, warnings at
  5/1 min, warnings re-arm and exhaustion can fire again after a top-up, day rollover zeroes.
- `policy`/`state`: JSON round-trip, missing file defaults, date rollover zeroes usage and bonus,
  lockout window.
- `pin`: verify/reject, corrupt input, lockout 5 → 5 min, doubling, 1 h cap.
- `policy`: `parseStartMenuApps` on a `Get-StartApps` sample — Store `!App` ids become
  `explorer.exe + shell:AppsFolder\…`, absolute `.exe` paths are kept, `Uninstall`/`ms-settings:`
  /shell-GUID junk is dropped, result sorted by name.
- No test needs a real child process, wall clock, admin rights or a PIN dialog.

## Running it

- Set up while logged in as the child: run `KidsMode.exe`, the parent sets the PIN, budget and
  apps. Put a shortcut in the child's Startup folder (`shell:startup`).
- Reset: delete `%APPDATA%\KidsMode\` and run again.
- Nothing is hidden: the budget and today's usage are in the two JSON files.
- A hard kill (`Stop-Process -Force`) skips `aboutToQuit`: the taskbar stays hidden — fix with
  `ShowWindow(FindWindow("Shell_TrayWnd"), 5)` — and `%TEMP%\KidsMode.lock` is left behind;
  delete it before the next start.
- The HUD is invisible to accessibility tools (`Qt::Tool`, no focus), so UI automation cannot
  click it; post `WM_LBUTTONDOWN/UP` to the `QWindowTool*` window instead (Home is at the right
  end of the 230 px strip, and a `WM_MOUSEMOVE` is what wakes it from 55% opacity).

## Qt Creator MCP server

Qt Creator runs an MCP server at `http://127.0.0.1:52697/` (Preferences > AI > Qt Creator MCP
Server), registered in `opencode.json`. The port is **pinned** (not Automatic) and the auth token is
**disabled** — loopback only — so the URL is stable across Qt Creator restarts and no token needs
copying. If it ever 401s again, someone re-enabled "Require an authentication token" there.
Do not commit `opencode.json` to a public repository. When connected, use it for configure, build
and test — it uses the kit Qt Creator has selected, which is the one that will actually build.

- `run_project` ties the app's lifetime to the MCP call: the app dies when the call returns or is
  interrupted. To keep KidsMode running, use `debugger_start` / `debugger_stop` — they return
  immediately and the app outlives the call.
- The Run button runs the *active* run config, which may be a test. `run_configure` with the
  KidsMode id to switch, and run the tests via `test_run`/ctest instead.
