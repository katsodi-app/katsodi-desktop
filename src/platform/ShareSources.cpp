#include "platform/ShareSources.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace katsodi {

#ifdef Q_OS_WIN
namespace {

constexpr int maxWindows = 40;
const QSize previewBox(480, 270);

bool shareable(HWND window, quint32 excludeProcess)
{
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
        return false;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (style & WS_EX_TOOLWINDOW)
        return false;
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == excludeProcess)
        return false;
    // Hidden UWP frames and windows on other virtual desktops are "cloaked".
    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked)
        return false;
    RECT rect;
    if (!GetWindowRect(window, &rect) || rect.right - rect.left < 120 || rect.bottom - rect.top < 80)
        return IsIconic(window); // minimized windows report a tiny rectangle
    return GetWindowTextLengthW(window) > 0;
}

QImage preview(HWND window)
{
    RECT rect;
    if (IsIconic(window) || !GetClientRect(window, &rect))
        return {};
    const int w = rect.right - rect.left, h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384)
        return {};
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage image;
    if (bitmap && bits) {
        HGDIOBJ old = SelectObject(memory, bitmap);
        // PW_RENDERFULLCONTENT also captures GPU-drawn windows (browsers, apps).
        if (PrintWindow(window, memory, PW_CLIENTONLY | PW_RENDERFULLCONTENT))
            image = QImage(static_cast<const uchar *>(bits), w, h, w * 4, QImage::Format_RGB32)
                        .scaled(previewBox, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        SelectObject(memory, old);
        DeleteObject(bitmap);
    }
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return image;
}

QImage icon(HWND window)
{
    DWORD_PTR handle = 0;
    // A hung application must not freeze the picker.
    SendMessageTimeoutW(window, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 100, &handle);
    if (!handle)
        SendMessageTimeoutW(window, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 100, &handle);
    if (!handle)
        handle = GetClassLongPtrW(window, GCLP_HICON);
    if (!handle)
        return {};
    return QImage::fromHICON(reinterpret_cast<HICON>(handle))
        .scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

struct Search {
    quint32 exclude;
    QList<HWND> windows;
};

BOOL CALLBACK collect(HWND window, LPARAM data)
{
    auto *search = reinterpret_cast<Search *>(data);
    if (shareable(window, search->exclude))
        search->windows.append(window);
    return search->windows.size() < maxWindows;
}

} // namespace

QList<ShareSource> listWindowSources(quint32 excludeProcess)
{
    Search search{excludeProcess, {}};
    EnumWindows(collect, reinterpret_cast<LPARAM>(&search));
    QList<ShareSource> sources;
    for (HWND window : search.windows) {
        wchar_t title[256] = {};
        GetWindowTextW(window, title, 256);
        ShareSource source;
        source.kind = ShareSource::Kind::Window;
        source.window = reinterpret_cast<quintptr>(window);
        source.title = QString::fromWCharArray(title).trimmed().left(120);
        source.minimized = IsIconic(window);
        source.preview = preview(window);
        source.icon = icon(window);
        if (!source.title.isEmpty())
            sources.append(source);
    }
    return sources;
}
quint32 windowProcess(quintptr window)
{
    DWORD process = 0;
    GetWindowThreadProcessId(reinterpret_cast<HWND>(window), &process);
    return process;
}
#else
QList<ShareSource> listWindowSources(quint32) { return {}; }
quint32 windowProcess(quintptr) { return 0; }
#endif

} // namespace katsodi
