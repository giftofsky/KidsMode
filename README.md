# Kids Mode

A Windows launcher that shows a child only the apps a parent approved, with a daily time
limit. When the time is up, the running app is closed and the screen locks until a parent types
the PIN. The parent can then add 15 minutes or quit.

**It is a boundary, not a lock.** It is a UI convenience, not parental control: Task Manager,
Ctrl+Alt+Del, another Windows account or a reboot can get around it. That is accepted.

## Use

1. Log in as the child, run `KidsMode.exe`. The setup asks for a 4-digit PIN, a daily limit and
   at least one app.
2. Put a shortcut to `KidsMode.exe` in the child's Startup folder (`Win+R`, `shell:startup`).
3. To change the limit or the apps: press the **Parent** button (or hold the time bar for two
   seconds) and enter the PIN. The same menu has **Quit Kids Mode**.
4. To reset everything: delete `%APPDATA%\KidsMode\` and run the app again.

Settings and today's usage are two readable JSON files in `%APPDATA%\KidsMode\`. Nothing is
hidden; a child who edits them has outgrown the tool. `kidsmode.log` in the same folder records
every state change, launch and child exit — read it first when something looks wrong.

Chrome and Edge are started with their own profile in `%APPDATA%\KidsMode\browser-profile`, so the
browser cannot hand the launch to an already-open window and disappear. That profile has no
bookmarks, history or logins from the parent's browser.

While an app runs, the launcher stays behind it as a plain cream backdrop instead of hiding, so
the desktop and its icons never show around a small window. Calculator and Notepad are Store apps
whose `exe` exits at once; Kids Mode follows the window they open and closes that window when the
time is up.

## Build

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=E:/Qt/6.11.0/msvc2022_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

LGPLv3: link Qt dynamically and ship the DLLs with `windeployqt`. Do not build Qt statically.
