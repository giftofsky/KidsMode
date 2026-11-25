#pragma once

#include <QIcon>

#include "policy.h"

// The icon for a tile: the real Store logo for apps launched through
// shell:AppsFolder (their exePath is only explorer.exe), the exe's icon
// otherwise. Cached, so calling it per paint is cheap.
QIcon appIcon(const AppTile &tile);
