#pragma once

#include <QImage>
#include <QList>
#include <QString>

namespace katsodi {

/**
 * What the presenter can share, as Discord's picker shows it: top-level
 * application windows (visible, titled, not tool windows, not cloaked, not
 * this application's own) and whole screens, each with a small preview and
 * the application's icon. Previews stay on this computer: they are shown in
 * the picker only and never sent anywhere.
 */
struct ShareSource {
    enum class Kind { Window, Screen };
    Kind kind = Kind::Window;
    quintptr window = 0; // HWND for windows
    int screen = -1;     // index in QGuiApplication::screens() for screens
    QString title;
    QImage preview;      // at most 480×270
    QImage icon;         // 32×32, may be null
    bool minimized = false;
};

/** Lists shareable windows with previews. Safe to call on a worker thread. */
QList<ShareSource> listWindowSources(quint32 excludeProcess);

} // namespace katsodi
