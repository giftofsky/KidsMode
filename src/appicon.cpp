#include "appicon.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QPixmap>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commoncontrols.h>

QIcon appIcon(const AppTile &tile)
{
    static QHash<QString, QIcon> cache;
    const QString key = tile.args.isEmpty() ? tile.exePath
                                            : tile.exePath + QLatin1Char(' ') + tile.args;
    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd())
        return cached.value();

    QIcon icon;
    const QString appsFolder = QStringLiteral("shell:AppsFolder\\");
    if (tile.args.startsWith(appsFolder)) {
        // The shell namespace knows the Store app's real logo; ask its folder item
        // for the shell icon index and take the jumbo (256 px) image from it.
        std::wstring name = (appsFolder + tile.args.mid(appsFolder.size())).toStdWString();
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(name.data(), nullptr, &pidl, 0, nullptr))) {
            SHFILEINFO info{};
            if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(pidl), 0, &info, sizeof(info),
                               SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_LARGEICON)) {
                IImageList *list = nullptr;
                if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_PPV_ARGS(&list)))) {
                    HICON hicon = nullptr;
                    if (SUCCEEDED(list->GetIcon(info.iIcon, ILD_TRANSPARENT, &hicon))) {
                        icon = QIcon(QPixmap::fromImage(QImage::fromHICON(hicon)));
                        DestroyIcon(hicon);
                    }
                    list->Release();
                }
            }
            CoTaskMemFree(pidl);
        }
    }

    if (icon.isNull()) {
        QFileIconProvider provider;
        const QFileInfo info(tile.exePath);
        icon = info.isFile() ? provider.icon(info) : provider.icon(QFileIconProvider::File);
    }
    cache.insert(key, icon);
    return icon;
}
