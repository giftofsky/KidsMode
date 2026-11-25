#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(kidsmode)

// Writes qCInfo/qCWarning to %APPDATA%\KidsMode\kidsmode.log, rotating at 1 MB.
void installLogger();
