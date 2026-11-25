#include "logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <cstdio>

Q_LOGGING_CATEGORY(kidsmode, "kidsmode")

namespace {

constexpr qint64 kMaxLogBytes = 1024 * 1024;
QFile g_log;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "INFO";
}

void handler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    const QString line =
        QStringLiteral("%1 %2 %3\n")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                 QString::fromLatin1(levelName(type)), message);

    if (g_log.isOpen()) {
        g_log.write(line.toUtf8());
        g_log.flush();
    } else {
        std::fputs(qPrintable(line), stderr);
    }
}

} // namespace

void installLogger()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/kidsmode.log");

    if (QFileInfo(path).size() > kMaxLogBytes) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }

    g_log.setFileName(path);
    (void)g_log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(handler);
}
