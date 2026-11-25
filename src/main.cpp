#include <QApplication>
#include <QDir>
#include <QLockFile>

#include "hudwindow.h"
#include "launcherwindow.h"
#include "logging.h"
#include "sessioncontroller.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("KidsMode"));
    installLogger();
    qCInfo(kidsmode) << "starting" << QCoreApplication::applicationFilePath();

    QLockFile lock(QDir::temp().filePath(QStringLiteral("KidsMode.lock")));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(100))
        return 0;

    LauncherWindow launcher;
    qCInfo(kidsmode) << "launcher built";
    HudWindow hud;
    qCInfo(kidsmode) << "hud built";
    SessionController controller(&launcher, &hud);
    qCInfo(kidsmode) << "controller built";
    if (!controller.start())
        return 0;
    qCInfo(kidsmode) << "controller started";

    const int code = app.exec();
    qCInfo(kidsmode) << "event loop returned" << code;
    return code;
}
