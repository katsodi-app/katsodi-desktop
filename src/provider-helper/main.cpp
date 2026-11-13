// katsodi-player: the provider-player helper (DESKTOP_APP_IMPLEMENTATION_PROMPT
// §19). A separate process that hosts exactly one official provider embed,
// drawn as a child window inside the client's stage.
//
// Engine: Microsoft Edge WebView2 (the runtime that ships with Windows 11),
// driven through its SDK. Qt WebEngine was tried first: its prebuilt
// binaries carry no H.264/AAC decoders, so Vimeo and SoundCloud could not
// play (measured, see docs/Features/Native Desktop Client). WebView2 has the
// codecs, is updated by Windows, and needs no browser runtime in our installer.
//
// - Started by the client only for a room whose source has an official
//   player API; it exits when its stdin closes, so it never outlives the
//   client and leaves no zombie.
// - IPC is the anonymous stdin/stdout pipe pair the client created: no
//   named endpoint another process could connect to. One JSON object per
//   line, at most 16 KiB, allow-listed commands only.
// - It receives no Katsodi token, secret or file access. The browser profile
//   is InPrivate in a temporary folder deleted on exit (provider cookies end
//   with the process). Only the provider-only page of the API origin may
//   load as the top document; new windows become an "external" event the
//   client may open in the system browser; every permission is denied;
//   downloads are cancelled; plain-http requests other than the page are
//   blocked.

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <windows.h>
#include <wrl.h>

#include <WebView2.h>
// The SDK header annotates a member with GCC's __attribute__ whenever
// __has_attribute exists, which MSVC also defines; the annotation is only
// meaningful for Chromium's own clang build.
#define __attribute__(x)
#include <WebView2EnvironmentOptions.h>
#undef __attribute__

#include <cstdio>
#include <functional>
#include <string>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace {

constexpr qsizetype maxLine = 16 * 1024;

bool acceptableOrigin(const QUrl &url)
{
    if (!url.isValid() || !url.path().isEmpty() || url.hasQuery() || !url.userInfo().isEmpty())
        return false;
    if (url.scheme() == QLatin1StringView("https"))
        return true;
    return url.scheme() == QLatin1StringView("http")
           && (url.host() == QLatin1StringView("127.0.0.1") || url.host() == QLatin1StringView("localhost"));
}

void emitLine(const QJsonObject &event)
{
    const QByteArray line = QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
    std::fflush(stdout);
}

void emitError(const QString &code)
{
    emitLine({{QStringLiteral("ev"), QStringLiteral("error")}, {QStringLiteral("code"), code}});
}

QString take(LPWSTR value)
{
    const QString out = value ? QString::fromWCharArray(value) : QString();
    CoTaskMemFree(value);
    return out;
}

// Reads stdin on its own thread and hands complete lines to the main thread.
class StdinReader : public QThread {
    Q_OBJECT
signals:
    void line(const QByteArray &line);
    void closed();

protected:
    void run() override
    {
        std::string buffer;
        char chunk[4096];
        DWORD got = 0;
        while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), chunk, sizeof chunk, &got, nullptr) && got > 0) {
            buffer.append(chunk, got);
            size_t end;
            while ((end = buffer.find('\n')) != std::string::npos) {
                if (end <= size_t(maxLine))
                    emit line(QByteArray(buffer.data(), qsizetype(end)));
                buffer.erase(0, end + 1);
            }
            if (buffer.size() > size_t(maxLine))
                buffer.clear(); // an over-long line is dropped, never executed
        }
        emit closed();
    }
};

LRESULT CALLBACK hostProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_ERASEBKGND) {
        RECT rect;
        GetClientRect(window, &rect);
        FillRect(HDC(w), &rect, HBRUSH(GetStockObject(BLACK_BRUSH)));
        return 1;
    }
    return DefWindowProcW(window, message, w, l);
}

QString jsString(const QString &value)
{
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

struct Player {
    HWND parent = nullptr;
    HWND host = nullptr;
    QUrl page;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> view;
    bool pageReady = false;
    bool visible = false;
    QJsonObject pendingLoad;
    QString lastState;
    RECT bounds{0, 0, 1, 1};

    bool isPage(const QString &uri) const
    {
        return QUrl(uri).adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment) == page;
    }

    void run(const QString &script, std::function<void(const QString &)> done = {})
    {
        if (!view)
            return;
        view->ExecuteScript(reinterpret_cast<LPCWSTR>(script.utf16()),
                            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                                [done](HRESULT result, LPCWSTR json) -> HRESULT {
            if (done && SUCCEEDED(result) && json)
                done(QString::fromWCharArray(json));
            return S_OK;
        }).Get());
    }

    void load(const QJsonObject &command)
    {
        const QString kind = command.value(QLatin1StringView("kind")).toString();
        const QString source = command.value(QLatin1StringView("source")).toString();
        static const QStringList kinds{QStringLiteral("youtube"), QStringLiteral("vimeo"), QStringLiteral("soundcloud")};
        if (!kinds.contains(kind) || source.size() > 2048)
            return;
        run(QStringLiteral("katsodi.load(%1, %2, %3, %4)")
                .arg(qint64(command.value(QLatin1StringView("generation")).toDouble()))
                .arg(jsString(kind), jsString(source),
                     command.value(QLatin1StringView("controls")).toBool() ? QStringLiteral("true") : QStringLiteral("false")));
    }

    void applyBounds()
    {
        if (!host)
            return;
        MoveWindow(host, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, TRUE);
        if (controller) {
            RECT inner{0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top};
            controller->put_Bounds(inner);
        }
    }

    void applyVisible()
    {
        if (host)
            ShowWindow(host, visible ? SW_SHOWNA : SW_HIDE);
        if (controller)
            controller->put_IsVisible(visible ? TRUE : FALSE);
    }

    void configure()
    {
        ComPtr<ICoreWebView2Settings> settings;
        view->get_Settings(&settings);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_IsWebMessageEnabled(FALSE);
        settings->put_IsBuiltInErrorPageEnabled(FALSE);
        ComPtr<ICoreWebView2Settings3> settings3;
        if (SUCCEEDED(settings.As(&settings3)))
            settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
        ComPtr<ICoreWebView2Settings4> settings4;
        if (SUCCEEDED(settings.As(&settings4))) {
            settings4->put_IsPasswordAutosaveEnabled(FALSE);
            settings4->put_IsGeneralAutofillEnabled(FALSE);
        }

        EventRegistrationToken token;
        // Only the provider-only page may be the top document.
        view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            if (!isPage(take(uri)))
                args->put_Cancel(TRUE);
            return S_OK;
        }).Get(), &token);
        view->add_FrameNavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            const QString scheme = QUrl(take(uri)).scheme();
            if (scheme != QLatin1StringView("https") && scheme != QLatin1StringView("about"))
                args->put_Cancel(TRUE);
            return S_OK;
        }).Get(), &token);
        view->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
            BOOL ok = FALSE;
            args->get_IsSuccess(&ok);
            pageReady = ok;
            if (!ok) {
                emitError(QStringLiteral("page"));
                return S_OK;
            }
            emitLine({{QStringLiteral("ev"), QStringLiteral("page")}});
            if (!pendingLoad.isEmpty()) {
                load(pendingLoad);
                pendingLoad = {};
            }
            return S_OK;
        }).Get(), &token);
        // "Watch on YouTube" and similar: the client decides, in the system browser.
        view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
            args->put_Handled(TRUE);
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            BOOL user = FALSE;
            args->get_IsUserInitiated(&user);
            const QUrl url(take(uri));
            if (user && url.scheme() == QLatin1StringView("https") && url.toString().size() <= 2048)
                emitLine({{QStringLiteral("ev"), QStringLiteral("external")}, {QStringLiteral("url"), url.toString()}});
            return S_OK;
        }).Get(), &token);
        view->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [](ICoreWebView2 *, ICoreWebView2PermissionRequestedEventArgs *args) -> HRESULT {
            args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
            return S_OK;
        }).Get(), &token);
        view->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>(
            [](ICoreWebView2 *, ICoreWebView2ProcessFailedEventArgs *) -> HRESULT {
            emitError(QStringLiteral("renderer"));
            return S_OK;
        }).Get(), &token);
        ComPtr<ICoreWebView2_4> view4;
        if (SUCCEEDED(view.As(&view4)))
            view4->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
                [](ICoreWebView2 *, ICoreWebView2DownloadStartingEventArgs *args) -> HRESULT {
                args->put_Cancel(TRUE);
                args->put_Handled(TRUE);
                return S_OK;
            }).Get(), &token);
        // Plain http is blocked except the loopback player page in development.
        view->AddWebResourceRequestedFilter(L"http://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        view->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
            ComPtr<ICoreWebView2WebResourceRequest> request;
            args->get_Request(&request);
            LPWSTR uri = nullptr;
            request->get_Uri(&uri);
            if (isPage(take(uri)))
                return S_OK;
            ComPtr<ICoreWebView2WebResourceResponse> response;
            environment->CreateWebResourceResponse(nullptr, 403, L"Blocked", L"", &response);
            args->put_Response(response.Get());
            return S_OK;
        }).Get(), &token);
    }
};

} // namespace

int main(int argc, char *argv[])
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("katsodi-player"));

    Player player;
    QUrl origin;
    const QStringList args = app.arguments();
    for (qsizetype i = 1; i + 1 < args.size(); i += 2) {
        if (args[i] == QLatin1StringView("--origin"))
            origin = QUrl(args[i + 1]);
        else if (args[i] == QLatin1StringView("--parent"))
            player.parent = HWND(quintptr(args[i + 1].toULongLong()));
    }
    if (!acceptableOrigin(origin) || !player.parent || !IsWindow(player.parent)) {
        std::fputs("katsodi-player: --origin <https or loopback origin> --parent <window> required\n", stderr);
        return 2;
    }
    player.page = origin;
    player.page.setPath(QStringLiteral("/api/native/v1/player"));

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return 3;

    WNDCLASSW type{};
    type.lpfnWndProc = hostProc;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"KatsodiPlayerHost";
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&type);
    // No WM_PARENTNOTIFY: the parent lives in another process and may be
    // waiting for this one to exit; a synchronous notification would deadlock.
    player.host = CreateWindowExW(WS_EX_NOPARENTNOTIFY, type.lpszClassName, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                  0, 0, 1, 1, player.parent, nullptr, type.hInstance, nullptr);
    if (!player.host) {
        emitError(QStringLiteral("parent"));
        return 3;
    }

    // InPrivate in a throwaway folder: nothing of the provider's session
    // survives the process.
    const QString dataFolder = QDir::tempPath() + QStringLiteral("/katsodi-player-%1").arg(GetCurrentProcessId());
    // Profiles of helpers that were killed (not closed) are swept here.
    for (const QFileInfo &stale : QDir(QDir::tempPath()).entryInfoList({QStringLiteral("katsodi-player-*")}, QDir::Dirs)) {
        bool ok = false;
        const DWORD pid = stale.fileName().mid(15).toULong(&ok);
        if (!ok || pid == GetCurrentProcessId())
            continue;
        if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
            const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
            CloseHandle(process);
            if (alive)
                continue;
        }
        QDir(stale.absoluteFilePath()).removeRecursively();
    }
    auto options = Make<CoreWebView2EnvironmentOptions>();
    QString flags = QStringLiteral("--autoplay-policy=no-user-gesture-required");
    if (qEnvironmentVariable("KATSODI_PLAYER_LOW") == QLatin1StringView("1"))
        flags += QStringLiteral(" --renderer-process-limit=1");
    options->put_AdditionalBrowserArguments(reinterpret_cast<LPCWSTR>(flags.utf16()));

    const HRESULT created = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(dataFolder).utf16()), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&player](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {
        if (FAILED(result) || !environment) {
            emitError(QStringLiteral("runtime"));
            QCoreApplication::exit(4);
            return S_OK;
        }
        player.environment = environment;
        ComPtr<ICoreWebView2Environment10> environment10;
        ComPtr<ICoreWebView2ControllerOptions> controllerOptions;
        if (SUCCEEDED(player.environment.As(&environment10))
            && SUCCEEDED(environment10->CreateCoreWebView2ControllerOptions(&controllerOptions)))
            controllerOptions->put_IsInPrivateModeEnabled(TRUE);
        auto done = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [&player](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
            if (FAILED(result) || !controller) {
                emitError(QStringLiteral("runtime"));
                QCoreApplication::exit(4);
                return S_OK;
            }
            player.controller = controller;
            player.controller->get_CoreWebView2(&player.view);
            ComPtr<ICoreWebView2Controller2> controller2;
            if (SUCCEEDED(player.controller.As(&controller2)))
                controller2->put_DefaultBackgroundColor({255, 0, 0, 0});
            player.configure();
            player.applyBounds();
            player.applyVisible();
            player.view->Navigate(reinterpret_cast<LPCWSTR>(player.page.toString().utf16()));
            emitLine({{QStringLiteral("ev"), QStringLiteral("started")}});
            return S_OK;
        });
        if (environment10 && controllerOptions)
            environment10->CreateCoreWebView2ControllerWithOptions(player.host, controllerOptions.Get(), done.Get());
        else
            player.environment->CreateCoreWebView2Controller(player.host, done.Get());
        return S_OK;
    }).Get());
    if (FAILED(created)) {
        // No WebView2 runtime on this machine.
        emitError(QStringLiteral("runtime"));
        return 4;
    }

    QTimer poll;
    poll.setInterval(500);
    QObject::connect(&poll, &QTimer::timeout, &app, [&player]() {
        if (!player.pageReady)
            return;
        player.run(QStringLiteral("window.katsodi ? katsodi.poll() : ''"), [&player](const QString &json) {
            // ExecuteScript returns the result JSON-encoded: a string here.
            const QJsonDocument outer = QJsonDocument::fromJson(QByteArray("[") + json.toUtf8() + ']');
            const QString text = outer.array().at(0).toString();
            if (text.isEmpty() || text.size() > 8192)
                return;
            const QJsonObject state = QJsonDocument::fromJson(text.toUtf8()).object();
            if (state.isEmpty())
                return;
            const bool events = !state.value(QLatin1StringView("events")).toArray().isEmpty();
            if (!events && text == player.lastState && !state.value(QLatin1StringView("playing")).toBool())
                return;
            player.lastState = text;
            QJsonObject out = state;
            out.insert(QStringLiteral("ev"), QStringLiteral("state"));
            emitLine(out);
        });
    });
    poll.start();

    StdinReader reader;
    QObject::connect(&reader, &StdinReader::closed, &app, &QCoreApplication::quit, Qt::QueuedConnection);
    QObject::connect(&reader, &StdinReader::line, &app, [&player](const QByteArray &line) {
        const QJsonObject command = QJsonDocument::fromJson(line).object();
        const QString op = command.value(QLatin1StringView("op")).toString();
        if (op == QLatin1StringView("load")) {
            if (player.pageReady)
                player.load(command);
            else
                player.pendingLoad = command;
        } else if (op == QLatin1StringView("play")) {
            player.run(QStringLiteral("katsodi.play()"));
        } else if (op == QLatin1StringView("pause")) {
            player.run(QStringLiteral("katsodi.pause()"));
        } else if (op == QLatin1StringView("seek")) {
            const double position = command.value(QLatin1StringView("position")).toDouble(-1);
            if (position >= 0 && position <= 172800)
                player.run(QStringLiteral("katsodi.seek(%1)").arg(position, 0, 'f', 3));
        } else if (op == QLatin1StringView("volume")) {
            const int value = command.value(QLatin1StringView("value")).toInt(-1);
            if (value >= 0 && value <= 100)
                player.run(QStringLiteral("katsodi.volume(%1)").arg(value));
        } else if (op == QLatin1StringView("unload")) {
            player.run(QStringLiteral("katsodi.unload()"));
        } else if (op == QLatin1StringView("geometry")) {
            // Logical pixels of the client window; WebView2 works in physical ones.
            const double scale = GetDpiForWindow(player.parent) / 96.0;
            const int x = command.value(QLatin1StringView("x")).toInt(), y = command.value(QLatin1StringView("y")).toInt();
            const int w = command.value(QLatin1StringView("w")).toInt(), h = command.value(QLatin1StringView("h")).toInt();
            if (w < 0 || h < 0 || w > 16384 || h > 16384 || qAbs(x) > 16384 || qAbs(y) > 16384)
                return;
            player.bounds = {LONG(qRound(x * scale)), LONG(qRound(y * scale)), LONG(qRound((x + qMax(1, w)) * scale)),
                             LONG(qRound((y + qMax(1, h)) * scale))};
            player.applyBounds();
        } else if (op == QLatin1StringView("visible")) {
            player.visible = command.value(QLatin1StringView("value")).toBool();
            player.applyVisible();
        }
    });
    reader.start();

    const int code = app.exec();
    poll.stop();
    if (player.controller)
        player.controller->Close();
    player.view.Reset();
    player.controller.Reset();
    player.environment.Reset();
    // The window is not destroyed explicitly: that would message the other
    // process's parent window synchronously. Process exit removes it.
    ShowWindow(player.host, SW_HIDE);
    CoUninitialize();
    // The browser processes release the folder shortly after Close().
    for (int attempt = 0; attempt < 10 && QDir(dataFolder).exists(); ++attempt) {
        if (QDir(dataFolder).removeRecursively())
            break;
        Sleep(100);
    }
    reader.wait(200);
    reader.terminate();
    return code;
}

#include "main.moc"
