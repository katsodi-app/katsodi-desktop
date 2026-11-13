#include "presentation/ScreenController.h"

#include "domain/Text.h"
#include "infrastructure/api/ApiClient.h"
#include "presentation/Preferences.h"
#include "presentation/RoomController.h"

#include <QGuiApplication>
#include <QJsonObject>
#include <QScreen>
#include <QUrl>
#include <QVideoFrame>

namespace katsodi {
namespace {

bool running(const QString &status)
{
    return status == QLatin1StringView("live") || status == QLatin1StringView("paused");
}

} // namespace

ScreenController::ScreenController(ApiClient *api, RoomController *room, Preferences *preferences,
                                   ScreenEngine *engine, QObject *parent)
    : QObject(parent), m_api(api), m_room(room), m_preferences(preferences), m_engine(engine)
{
    m_beat.setInterval(20000);
    connect(&m_beat, &QTimer::timeout, this, &ScreenController::beat);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &ScreenController::connectDemo);

    connect(m_room, &RoomController::stateChanged, this, &ScreenController::roomChanged);
    connect(m_room, &RoomController::roomChanged, this, &ScreenController::roomChanged);

    connect(m_engine, &ScreenEngine::connected, this, [this](quint64 generation) {
        if (generation != m_generation)
            return;
        m_connected = generation;
        m_retries = 0;
        setState(QStringLiteral("connected"));
    });
    connect(m_engine, &ScreenEngine::connectFailed, this, [this](quint64 generation) {
        if (generation != m_generation)
            return;
        setState(QStringLiteral("error"), QStringLiteral("screenUnavailable"));
    });
    connect(m_engine, &ScreenEngine::ended, this, [this](quint64 generation, ScreenEngine::Ended reason) {
        if (generation != m_generation)
            return;
        m_connected = 0;
        stopCapture();
        // An unexplained drop is retried with a fresh token (2, 5, 10 s).
        if (reason == ScreenEngine::Ended::Network && m_retries < 3) {
            static constexpr int delays[] = {2000, 5000, 10000};
            m_retry.start(delays[m_retries++]);
            setState(QStringLiteral("connecting"));
            return;
        }
        setState(QStringLiteral("off"));
    });
    connect(m_engine, &ScreenEngine::remoteScreenChanged, this, [this](bool present, QSize size) {
        m_remote = present;
        if (present && size.height() > 0)
            m_remoteAspect = qreal(size.width()) / size.height();
        emit changed();
    });
    connect(m_engine, &ScreenEngine::publishingChanged, this, [this](bool publishing) {
        if (!publishing && m_sharing)
            stopCapture();
    });
    connect(m_engine, &ScreenEngine::publishFailed, this, [this]() {
        stopCapture();
        setState(m_state, QStringLiteral("screenShareFailed"));
    });

    m_capture.setVideoSink(&m_captureSink);
    connect(&m_captureSink, &QVideoSink::videoFrameChanged, this, &ScreenController::onFrame);
    connect(&m_screenCapture, &QScreenCapture::errorOccurred, this, [this](QScreenCapture::Error, const QString &) {
        if (m_sharing) {
            stopSharing();
            setState(m_state, QStringLiteral("screenCaptureFailed"));
        }
    });
    // A shared window that closes ends the presentation.
    connect(&m_windowCapture, &QWindowCapture::errorOccurred, this, [this](QWindowCapture::Error, const QString &) {
        if (m_sharing) {
            stopSharing();
            setState(m_state, QStringLiteral("screenCaptureFailed"));
        }
    });
}

ScreenController::~ScreenController() { shutdown(); }

QSize ScreenController::publishedSize(QSize captured, bool low)
{
    if (captured.width() <= 0 || captured.height() <= 0)
        return {};
    const QSize box = low ? QSize(1280, 720) : QSize(1920, 1080);
    QSize size = captured.width() > box.width() || captured.height() > box.height()
                     ? captured.scaled(box, Qt::KeepAspectRatio)
                     : captured;
    // WebRTC crops to multiples of 4 (measured: 822 arrived as 820), so
    // publish aligned sizes and receive exactly what was sent.
    size.setWidth(qMax(4, size.width() & ~3));
    size.setHeight(qMax(4, size.height() & ~3));
    return size;
}

bool ScreenController::presenter() const
{
    return m_room->joined() && m_room->demoMine() && running(m_room->demoStatus());
}

void ScreenController::setState(const QString &state, const QString &errorKey)
{
    m_state = state;
    m_errorKey = errorKey;
    emit changed();
}

void ScreenController::setWanted(bool wanted)
{
    if (wanted == m_wanted)
        return;
    m_wanted = wanted;
    roomChanged();
}

void ScreenController::setVideoSink(QVideoSink *sink) { m_engine->setVideoSink(sink); }

void ScreenController::roomChanged()
{
    const bool open = m_room->joined() && !m_room->closed() && !m_room->roomId().isEmpty();
    const bool active = open && running(m_room->demoStatus());
    if (m_room->roomId() != m_roomId && !m_roomId.isEmpty()) {
        disconnectDemo(); // another room, or none
    }
    if (!active) {
        if (!m_roomId.isEmpty())
            disconnectDemo();
        m_beat.stop();
        emit changed();
        return;
    }
    // The presenter confirms its demo, sharing or not yet.
    if (presenter() && !m_beat.isActive()) {
        m_beat.start();
        beat();
    } else if (!presenter()) {
        m_beat.stop();
        if (m_sharing)
            stopCapture();
    }
    m_engine->setPaused(m_room->demoStatus() == QLatin1StringView("paused"));
    if ((m_wanted || m_sharing || presenter()) && m_roomId.isEmpty())
        connectDemo();
    emit changed();
}

void ScreenController::connectDemo()
{
    if (m_room->roomId().isEmpty() || !m_engine->available())
        return;
    m_roomId = m_room->roomId();
    const quint64 generation = ++m_generation;
    setState(QStringLiteral("connecting"));
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/demo/token").arg(m_roomId), {}, this,
                [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation)
            return;
        if (error) {
            setState(QStringLiteral("error"),
                     error->code == QLatin1StringView("not_configured") ? QStringLiteral("screenUnavailable")
                                                                        : error->messageKey);
            // No retry on every poll: a new demo or room tries again.
            return;
        }
        const QString url = body.value(QLatin1StringView("url")).toString();
        const QString token = body.value(QLatin1StringView("token")).toString();
        const QUrl parsed(url);
        if ((parsed.scheme() != QLatin1StringView("wss") && parsed.scheme() != QLatin1StringView("https"))
            || token.isEmpty() || token.size() > 8192) {
            setState(QStringLiteral("error"), QStringLiteral("errorServer"));
            return;
        }
        m_mayPublish = body.value(QLatin1StringView("publish")).toBool();
        m_engine->connectRoom(generation, url, token);
    });
}

void ScreenController::disconnectDemo()
{
    stopCapture();
    m_retry.stop();
    m_retries = 0;
    ++m_generation;
    m_connected = 0;
    m_roomId.clear();
    m_mayPublish = false;
    m_engine->disconnectRoom();
    m_remote = false;
    setState(QStringLiteral("off"));
}

void ScreenController::shutdown()
{
    m_beat.stop();
    stopCapture();
    m_engine->disconnectRoom();
}

void ScreenController::beat()
{
    const QString roomId = m_room->roomId();
    if (roomId.isEmpty())
        return;
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/demo/beat").arg(roomId), {}, this,
                [this, roomId](const QJsonObject &, const ApiError *error) {
        // A refusal means the demo was ended (by a host or by time): stop.
        if (error && !error->retryable && roomId == m_room->roomId()) {
            m_beat.stop();
            stopCapture();
        }
    });
}

void ScreenController::refreshSources()
{
    m_sourceList.clear();
    m_screens.clear();
    m_windows.clear();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        m_screens.append(screen);
        const QSize size = screen->size() * screen->devicePixelRatio();
        m_sourceList.append(QVariantMap{{QStringLiteral("index"), m_sourceList.size()},
                                        {QStringLiteral("kind"), QStringLiteral("screen")},
                                        {QStringLiteral("name"), QStringLiteral("%1 · %2×%3")
                                                                     .arg(screen->name().left(60))
                                                                     .arg(size.width())
                                                                     .arg(size.height())}});
    }
    for (const QCapturableWindow &window : QWindowCapture::capturableWindows()) {
        const QString title = window.description().trimmed();
        if (title.isEmpty() || !window.isValid())
            continue;
        m_windows.append(window);
        m_sourceList.append(QVariantMap{{QStringLiteral("index"), m_sourceList.size()},
                                        {QStringLiteral("kind"), QStringLiteral("window")},
                                        {QStringLiteral("name"), title.left(120)}});
        if (m_windows.size() >= 60)
            break;
    }
    emit sourcesChanged();
}

void ScreenController::share(int index)
{
    if (!presenter() || m_sharing || index < 0 || index >= m_sourceList.size())
        return;
    // A connection that may not publish (made before this viewer became the
    // presenter, or failed) is replaced by a new one with a fresh grant.
    if (!m_roomId.isEmpty() && (!m_connected || !m_mayPublish))
        disconnectDemo();
    const QVariantMap source = m_sourceList.at(index).toMap();
    m_capture.setScreenCapture(nullptr);
    m_capture.setWindowCapture(nullptr);
    if (source.value(QStringLiteral("kind")) == QLatin1StringView("screen")) {
        QScreen *screen = m_screens.value(index);
        if (!screen)
            return;
        m_screenCapture.setScreen(screen);
        m_capture.setScreenCapture(&m_screenCapture);
        m_screenCapture.start();
    } else {
        const qsizetype window = index - m_screens.size();
        if (window < 0 || window >= m_windows.size() || !m_windows.at(window).isValid())
            return;
        m_windowCapture.setWindow(m_windows.at(window));
        m_capture.setWindowCapture(&m_windowCapture);
        m_windowCapture.start();
    }
    m_sharing = true;
    m_publishStarted = false;
    m_sinceFrame.invalidate();
    setState(m_state); // clears an earlier error
    if (m_roomId.isEmpty())
        connectDemo();
}

void ScreenController::onFrame(const QVideoFrame &frame)
{
    if (!m_sharing || !frame.isValid() || !m_connected || !m_mayPublish)
        return;
    const bool low = m_preferences->lowResources();
    const qint64 interval = 1000 / (low ? 8 : 15);
    if (m_sinceFrame.isValid() && m_sinceFrame.elapsed() < interval)
        return;
    if (m_room->demoStatus() == QLatin1StringView("paused"))
        return;
    m_sinceFrame.start();
    const QImage image = frame.toImage();
    if (image.isNull())
        return;
    if (!m_publishStarted) {
        m_publishStarted = true;
        m_engine->startPublishing(publishedSize(image.size(), low), low ? 8 : 15, low ? 1'200'000 : 2'500'000);
    }
    m_engine->pushFrame(image);
}

void ScreenController::stopCapture()
{
    const bool was = m_sharing;
    m_sharing = false;
    m_screenCapture.stop();
    m_windowCapture.stop();
    m_capture.setScreenCapture(nullptr);
    m_capture.setWindowCapture(nullptr);
    if (m_publishStarted)
        m_engine->stopPublishing();
    m_publishStarted = false;
    if (was)
        emit changed();
}

void ScreenController::stopSharing()
{
    stopCapture();
    stop();
}

void ScreenController::post(const QString &suffix, const QJsonObject &body, bool remove)
{
    const QString roomId = m_room->roomId();
    if (roomId.isEmpty() || m_busy)
        return;
    m_busy = true;
    m_errorKey.clear();
    emit changed();
    auto done = [this](const QJsonObject &, const ApiError *error) {
        m_busy = false;
        if (error)
            m_errorKey = error->code == QLatin1StringView("demo_busy") ? QStringLiteral("demoBusy") : error->messageKey;
        emit changed();
        m_room->retry(); // show the new demo state at once
    };
    const QString path = QStringLiteral("/api/native/v1/rooms/%1/demo%2").arg(roomId, suffix);
    if (remove)
        m_api->del(path, this, done);
    else
        m_api->post(path, body, this, done);
}

bool ScreenController::request(const QString &url)
{
    if (!m_room->joined() || !isAcceptableSource(url))
        return false;
    post({}, QJsonObject{{QStringLiteral("url"), url.trimmed()}});
    return true;
}

void ScreenController::decide(bool accept)
{
    if (m_room->canControl())
        post(QStringLiteral("/decision"), QJsonObject{{QStringLiteral("accept"), accept}});
}

void ScreenController::togglePause()
{
    if (m_room->canControl() && running(m_room->demoStatus()))
        post(QStringLiteral("/pause"),
             QJsonObject{{QStringLiteral("paused"), m_room->demoStatus() == QLatin1StringView("live")}});
}

void ScreenController::stop()
{
    if (!m_room->demoStatus().isEmpty())
        post({}, {}, true);
}

} // namespace katsodi
