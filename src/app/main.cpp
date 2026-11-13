// Katsodi desktop client: application-owned Qt Quick interface. It renders
// no website pages; shared data comes from /api/native/v1 on the server.
#include "BuildConfig.h"
#include "application/Session.h"
#include "infrastructure/api/ApiClient.h"
#include "infrastructure/credentials/CredentialStore.h"
#include "presentation/ChatModel.h"
#include "presentation/FandomModel.h"
#include "presentation/Preferences.h"
#include "presentation/RoomController.h"
#include "presentation/RoomListModel.h"
#include "presentation/Strings.h"
#include "presentation/ThemeController.h"
#include "presentation/VoiceController.h"
#include "presentation/PlayerController.h"
#include "presentation/ScreenController.h"
#include "app/PosterImageProvider.h"
#include "presentation/SearchModel.h"
#include "infrastructure/storage/LocalLibrary.h"
#ifdef KATSODI_WITH_LIVEKIT
#include "media/livekit/LiveKitVoiceEngine.h"
#include "media/livekit/LiveKitScreenEngine.h"
#endif

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QQmlContext>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <memory>
#include <QUrl>

using namespace katsodi;

namespace {

/** Production: fixed origin. Development: a bare loopback origin only. */
QUrl apiOrigin()
{
    if (!build::development)
        return QUrl(QString::fromLatin1(build::productionOrigin));
    const QString value = qEnvironmentVariable("KATSODI_API_ORIGIN");
    const QUrl url(value.isEmpty() ? QStringLiteral("http://127.0.0.1:8787") : value);
    const bool loopback = url.host() == QLatin1StringView("127.0.0.1") || url.host() == QLatin1StringView("localhost");
    if (url.isValid() && (url.scheme() == QLatin1StringView("http") || url.scheme() == QLatin1StringView("https"))
        && loopback && url.userInfo().isEmpty() && (url.path().isEmpty() || url.path() == QLatin1StringView("/"))
        && !url.hasQuery())
        return url.adjusted(QUrl::RemovePath);
    qWarning("KATSODI_API_ORIGIN must be a loopback origin; using http://127.0.0.1:8787");
    return QUrl(QStringLiteral("http://127.0.0.1:8787"));
}

/** Opens only http(s) addresses in the system browser. */
class Links : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool openExternal(const QString &address) const
    {
        const QUrl url(address, QUrl::StrictMode);
        if (!url.isValid() || address.size() > 2048 || !url.userInfo().isEmpty()
            || (url.scheme() != QLatin1StringView("https") && url.scheme() != QLatin1StringView("http")))
            return false;
        return QDesktopServices::openUrl(url);
    }
    Q_INVOKABLE QString host(const QString &address) const { return QUrl(address).host(); }
};

void restoreGeometry(QQuickWindow *window, const Preferences &preferences)
{
    const QRect saved = preferences.windowGeometry();
    bool visible = false;
    for (const QScreen *screen : QGuiApplication::screens())
        if (saved.isValid() && screen->availableGeometry().intersected(saved).width() >= saved.width() / 2
            && screen->availableGeometry().intersected(saved).height() >= saved.height() / 2)
            visible = true;
    if (visible)
        window->setGeometry(saved);
    else if (const QScreen *primary = QGuiApplication::primaryScreen()) {
        const QRect area = primary->availableGeometry();
        const QSize size(std::min(1280, area.width()), std::min(800, area.height()));
        window->setGeometry(QRect(area.center() - QPoint(size.width() / 2, size.height() / 2), size));
    }
    if (preferences.windowMaximized())
        window->showMaximized();
    else
        window->show();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    const QString application = build::development ? QStringLiteral("Katsodi Dev") : QStringLiteral("Katsodi");
    QGuiApplication::setOrganizationName(QStringLiteral("Katsodi"));
    QGuiApplication::setApplicationName(application);
    QGuiApplication::setApplicationVersion(QString::fromLatin1(build::version));

    // One instance per user: a second launch asks the first to come forward.
    const QString instance = QStringLiteral("katsodi-%1-%2").arg(QString::fromLatin1(build::channel),
                                                                 qEnvironmentVariable("USERNAME"));
    QLockFile lock(QDir::temp().filePath(instance + QStringLiteral(".lock")));
    if (!lock.tryLock(100)) {
        QLocalSocket socket;
        socket.connectToServer(instance);
        if (socket.waitForConnected(1000)) {
            socket.write("focus");
            socket.waitForBytesWritten(1000);
        }
        return 0;
    }
    QLocalServer::removeServer(instance);
    QLocalServer focusServer;
    focusServer.setSocketOptions(QLocalServer::UserAccessOption);
    focusServer.listen(instance);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Preferences preferences(application);
    ThemeController theme(&preferences);
    Strings strings(&preferences);
    ApiClient api(apiOrigin());
    Session session(&api, CredentialStore(application + QStringLiteral("/session")));
    api.setTokenSource(&session);
    const QUrl artwork = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QStringLiteral("/artwork/fandoms/"));
    FandomModel fandoms(artwork);
    RoomListModel rooms(&api);
    RoomController room(&api, &session, &preferences);
#ifdef KATSODI_WITH_LIVEKIT
    LiveKitVoiceEngine voiceEngine;
#else
    NullVoiceEngine voiceEngine;
#endif
    VoiceController voice(&api, &room, &voiceEngine);
#ifdef KATSODI_WITH_LIVEKIT
    LiveKitScreenEngine screenEngine;
#else
    NullScreenEngine screenEngine;
#endif
    ScreenController screen(&api, &room, &preferences, &screenEngine);
    SearchModel search(&rooms, &fandoms);
    LocalLibrary library(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                         + QStringLiteral("/library.sqlite"));
    Links links;
#ifdef KATSODI_WITH_PLAYER
    const QString playerHelper = QCoreApplication::applicationDirPath() + QStringLiteral("/katsodi-player.exe");
#else
    const QString playerHelper;
#endif
    PlayerController player(&room, &preferences, apiOrigin(),
                            QFileInfo::exists(playerHelper) ? playerHelper : QString());

    QGuiApplication::setFont(QFont(theme.fontFamily(), 10));

    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Theme", &theme);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "I18n", &strings);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Prefs", &preferences);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Account", &session);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Fandoms", &fandoms);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Rooms", &rooms);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Room", &room);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Voice", &voice);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Search", &search);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Library", &library);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Links", &links);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Player", &player);
    qmlRegisterSingletonInstance("Katsodi.App", 1, 0, "Demo", &screen);
    qmlRegisterUncreatableType<ChatModel>("Katsodi.App", 1, 0, "ChatModel", QStringLiteral("owned by Room"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("poster"),
                            new PosterImageProvider(QCoreApplication::applicationDirPath() + QStringLiteral("/artwork")));
    engine.rootContext()->setContextProperty(QStringLiteral("appVersion"), QString::fromLatin1(build::version));
    engine.rootContext()->setContextProperty(QStringLiteral("appChannel"), QString::fromLatin1(build::channel));
    engine.rootContext()->setContextProperty(
        QStringLiteral("editorialBase"),
        QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QStringLiteral("/artwork/editorial/")));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Katsodi.Ui", "Main");
    auto *window = engine.rootObjects().isEmpty() ? nullptr
                                                   : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    restoreGeometry(window, preferences);

    QObject::connect(&focusServer, &QLocalServer::newConnection, window, [&focusServer, window] {
        while (QLocalSocket *socket = focusServer.nextPendingConnection()) {
            socket->deleteLater();
            if (window->windowStates() & Qt::WindowMinimized)
                window->showNormal();
            window->raise();
            window->requestActivate();
        }
    });
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&] {
        // Quitting ends the room's polling and pings with the process.
        // Quitting ends the call first: microphone and speakers are released.
        voice.leave();
        player.shutdown();
        screen.shutdown();
        room.close();
        const bool maximized = window->windowStates() & Qt::WindowMaximized;
        if (!maximized)
            preferences.saveWindow(window->geometry(), false);
        else
            preferences.saveWindow(preferences.windowGeometry(), true);
    });

    session.start();
    rooms.reload();

    // Development builds only: KATSODI_CAPTURE=<folder> saves a screenshot of
    // each route in KATSODI_CAPTURE_ROUTES (comma-separated) and quits. Used
    // for visual checks of the native screens; never compiled into releases.
    if constexpr (build::development) {
        const QString folder = qEnvironmentVariable("KATSODI_CAPTURE");
        if (!folder.isEmpty()) {
            const QStringList routes = qEnvironmentVariable("KATSODI_CAPTURE_ROUTES",
                                                            QStringLiteral("home,rooms,fandoms,settings,signin"))
                                           .split(u',', Qt::SkipEmptyParts);
            // Optional: palette, language and window size for this run only.
            const QString originalTheme = preferences.theme(), originalLocale = preferences.locale();
            if (qEnvironmentVariableIsSet("KATSODI_CAPTURE_THEME"))
                preferences.setTheme(qEnvironmentVariable("KATSODI_CAPTURE_THEME"));
            if (qEnvironmentVariableIsSet("KATSODI_CAPTURE_LOCALE"))
                preferences.setLocale(qEnvironmentVariable("KATSODI_CAPTURE_LOCALE"));
            const QStringList size = qEnvironmentVariable("KATSODI_CAPTURE_SIZE").split(u'x');
            if (size.size() == 2)
                window->resize(size[0].toInt(), size[1].toInt());
            QObject::connect(&app, &QCoreApplication::aboutToQuit, [&preferences, originalTheme, originalLocale] {
                preferences.setTheme(originalTheme);
                preferences.setLocale(originalLocale);
            });
            auto *timer = new QTimer(&app);
            auto step = std::make_shared<int>(-1);
            QObject::connect(timer, &QTimer::timeout, &app, [=] {
                if (*step >= 0 && *step < routes.size()) {
                    const QString name = routes[*step].section(u':', 0, 0);
                    window->grabWindow().save(QDir(folder).filePath(name.section(u'=', 0, 0) + QStringLiteral(".png")));
                }
                ++*step;
                if (*step >= routes.size()) {
                    QCoreApplication::quit();
                    return;
                }
                // "name" or "name:id", optional "=theme/locale" suffix handled by the caller.
                const QString spec = routes[*step];
                const QString route = spec.section(u':', 0, 0);
                const QString id = spec.section(u':', 1, 1);
                QMetaObject::invokeMethod(window, "captureStep", Q_ARG(QVariant, route), Q_ARG(QVariant, id));
            });
            timer->start(3500);
        }
    }
    return QGuiApplication::exec();
}

#include "main.moc"
