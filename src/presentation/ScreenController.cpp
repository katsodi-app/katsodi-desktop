#include "presentation/ScreenController.h"

#include "domain/Text.h"
#include "infrastructure/api/ApiClient.h"
#include "presentation/Preferences.h"
#include "presentation/BrowserController.h"
#include "presentation/RoomController.h"

#include "platform/ShareSources.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QUrl>
#include <QVideoFrame>
#include <QWindow>
#include <thread>

namespace katsodi {
namespace {

bool running(const QString &status)
{
    return status == QLatin1StringView("live") || status == QLatin1StringView("paused");
}

} // namespace

ScreenController::ScreenController(ApiClient *api, RoomController *room, Preferences *preferences,
                                   ScreenEngine *engine, BrowserController *browser, QObject *parent)
    : QObject(parent), m_api(api), m_room(room), m_preferences(preferences), m_engine(engine), m_browser(browser)
{
    m_clock.start();
    m_announce.setInterval(5000); // late joiners learn who may control
    connect(&m_announce, &QTimer::timeout, this, &ScreenController::announceControl);
    connect(m_engine, &ScreenEngine::dataReceived, this, &ScreenController::onData);
    connect(m_engine, &ScreenEngine::remoteAudioChanged, this, [this](bool present) {
        m_remoteAudio = present;
        emit audioChanged();
    });
    connect(&m_audio, &ProcessAudio::failed, this, [this] {
        m_engine->stopAudio();
        m_audioState = QStringLiteral("unavailable");
        emit audioChanged();
    });
    if (m_browser) {
        connect(m_browser, &BrowserController::frame, this, [this](const QImage &image) {
            if (m_browserSession && m_sharing)
                pushImage(image);
        });
        connect(m_browser, &BrowserController::failed, this, [this] {
            if (m_browserSession)
                stopSharing();
        });
    }
    m_quality = m_preferences->lowResources() ? 720 : 1080;
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

QSize ScreenController::publishedSize(QSize captured, int boxHeight)
{
    if (captured.width() <= 0 || captured.height() <= 0)
        return {};
    const QSize box = boxHeight >= 1080 ? QSize(1920, 1080) : QSize(1280, 720);
    QSize size = captured.width() > box.width() || captured.height() > box.height()
                     ? captured.scaled(box, Qt::KeepAspectRatio)
                     : captured;
    // WebRTC crops to multiples of 4 (measured: 822 arrived as 820), so
    // publish aligned sizes and receive exactly what was sent.
    size.setWidth(qMax(4, size.width() & ~3));
    size.setHeight(qMax(4, size.height() & ~3));
    return size;
}

int ScreenController::bitrateFor(int boxHeight, int framesPerSecond)
{
    const bool hd = boxHeight >= 1080, smooth = framesPerSecond >= 30;
    return hd ? (smooth ? 4'000'000 : 2'500'000) : (smooth ? 2'500'000 : 1'500'000);
}

void ScreenController::setQuality(int value)
{
    value = value >= 1080 ? 1080 : 720;
    if (value != m_quality) {
        m_quality = value;
        emit qualityChanged();
    }
}

void ScreenController::setFramesPerSecond(int value)
{
    value = value >= 30 ? 30 : 15;
    if (value != m_framesPerSecond) {
        m_framesPerSecond = value;
        emit qualityChanged();
    }
}

void ScreenController::setPreviewSink(QVideoSink *sink) { m_previewSink = sink; }

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
        m_pendingBrowser.clear();
        m_canControl = false;
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
    if (presenter() && !m_pendingBrowser.isEmpty() && !m_sharing)
        beginBrowser();
    // The data grant is in the token: a viewer reconnects when a host opens
    // or closes the channel, to send (or stop sending) input.
    if (m_room->demoChannelOpen() != m_lastChannelOpen) {
        m_lastChannelOpen = m_room->demoChannelOpen();
        if (!presenter() && !m_room->canControl() && !m_roomId.isEmpty()) {
            disconnectDemo();
            connectDemo();
        }
    }
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

namespace {
QString dataUrl(const QImage &image, const char *format, int quality = -1)
{
    if (image.isNull())
        return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format, quality);
    return QStringLiteral("data:image/%1;base64,").arg(QString::fromLatin1(format).toLower())
           + QString::fromLatin1(bytes.toBase64());
}
} // namespace

void ScreenController::refreshSources()
{
    // Screens at once (a grab of the user's own screens, shown only in the
    // picker); windows on a worker thread, since PrintWindow may be slow.
    m_sources.clear();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        ShareSource source;
        source.kind = ShareSource::Kind::Screen;
        source.screen = i;
        const QSize size = screens[i]->size() * screens[i]->devicePixelRatio();
        source.title = QStringLiteral("%1 · %2×%3").arg(screens[i]->name().left(60)).arg(size.width()).arg(size.height());
        source.preview = screens[i]->grabWindow(0).toImage().scaled(480, 270, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_sources.append(source);
    }
    publishSources();
    m_loadingSources = true;
    emit sourcesChanged();
    const quint64 generation = ++m_sourcesGeneration;
    const quint32 self = quint32(QCoreApplication::applicationPid());
    QPointer<ScreenController> guard(this);
    std::thread([guard, generation, self] {
        QList<ShareSource> windows = listWindowSources(self);
        QMetaObject::invokeMethod(qApp, [guard, generation, windows = std::move(windows)]() mutable {
            if (!guard || generation != guard->m_sourcesGeneration)
                return;
            guard->m_sources.append(windows);
            guard->m_loadingSources = false;
            guard->publishSources();
            emit guard->sourcesChanged();
        });
    }).detach();
}

void ScreenController::publishSources()
{
    m_sourceList.clear();
    for (int i = 0; i < m_sources.size(); ++i) {
        const ShareSource &source = m_sources.at(i);
        m_sourceList.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("kind"), source.kind == ShareSource::Kind::Screen ? QStringLiteral("screen") : QStringLiteral("window")},
            {QStringLiteral("name"), source.title},
            {QStringLiteral("preview"), dataUrl(source.preview, "JPEG", 80)},
            {QStringLiteral("icon"), dataUrl(source.icon, "PNG")},
            {QStringLiteral("minimized"), source.minimized}});
    }
}

void ScreenController::share(int index)
{
    if (!presenter() || m_sharing || index < 0 || index >= m_sources.size())
        return;
    // A connection that may not publish (made before this viewer became the
    // presenter, or failed) is replaced by a new one with a fresh grant.
    if (!m_roomId.isEmpty() && (!m_connected || !m_mayPublish))
        disconnectDemo();
    const ShareSource source = m_sources.at(index);
    m_capture.setScreenCapture(nullptr);
    m_capture.setWindowCapture(nullptr);
    m_foreign.reset();
    if (source.kind == ShareSource::Kind::Screen) {
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (source.screen < 0 || source.screen >= screens.size())
            return;
        m_screenCapture.setScreen(screens.at(source.screen));
        m_capture.setScreenCapture(&m_screenCapture);
        m_screenCapture.start();
    } else {
        // Qt captures a window given as a QWindow; a foreign one wraps the handle.
        m_foreign.reset(QWindow::fromWinId(WId(source.window)));
        const QCapturableWindow window(m_foreign.get());
        if (!m_foreign || !window.isValid()) {
            m_foreign.reset();
            setState(m_state, QStringLiteral("screenCaptureFailed"));
            return;
        }
        m_windowCapture.setWindow(window);
        m_capture.setWindowCapture(&m_windowCapture);
        m_windowCapture.start();
    }
    m_shareProcess = source.kind == ShareSource::Kind::Window ? windowProcess(source.window) : 0;
    m_sharing = true;
    m_publishStarted = false;
    m_sinceFrame.invalidate();
    setState(m_state); // clears an earlier error
    if (m_roomId.isEmpty())
        connectDemo();
}

void ScreenController::onFrame(const QVideoFrame &frame)
{
    if (!m_sharing || !frame.isValid())
        return;
    const qint64 interval = 1000 / m_framesPerSecond;
    if (m_sinceFrame.isValid() && m_sinceFrame.elapsed() < interval)
        return;
    m_sinceFrame.start();
    // The presenter sees what is shared, even before the connection is up.
    if (m_previewSink)
        m_previewSink->setVideoFrame(frame);
    if (!m_connected || !m_mayPublish || m_room->demoStatus() == QLatin1StringView("paused"))
        return;
    pushImage(frame.toImage());
}

void ScreenController::pushImage(const QImage &image)
{
    if (image.isNull() || !m_connected || !m_mayPublish || m_room->demoStatus() == QLatin1StringView("paused"))
        return;
    if (!m_publishStarted) {
        m_publishStarted = true;
        m_engine->startPublishing(publishedSize(image.size(), m_quality), m_framesPerSecond,
                                  bitrateFor(m_quality, m_framesPerSecond));
        startSound();
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
    m_foreign.reset();
    stopSound();
    if (m_browserSession && m_browser)
        m_browser->stop();
    if (m_browserSession) {
        m_browserSession = false;
        m_controllers.clear();
        m_controlAll = false;
        m_announce.stop();
        emit controlChanged();
    }
    if (m_previewSink)
        m_previewSink->setVideoFrame({});
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

bool ScreenController::startBrowser(const QString &url)
{
    if (!m_browser || !m_browser->available() || !m_room->canControl() || !m_room->demoStatus().isEmpty()
        || !isAcceptableSource(url))
        return false;
    m_pendingBrowser = url.trimmed();
    request(m_pendingBrowser);
    return true;
}

void ScreenController::beginBrowser()
{
    const QString url = std::exchange(m_pendingBrowser, {});
    QWindow *window = QGuiApplication::focusWindow();
    if (!window && !QGuiApplication::topLevelWindows().isEmpty())
        window = QGuiApplication::topLevelWindows().first();
    if (!presenter() || !m_browser || !window)
        return;
    if (!m_roomId.isEmpty() && (!m_connected || !m_mayPublish))
        disconnectDemo();
    const QSize box = m_quality >= 1080 ? QSize(1920, 1080) : QSize(1280, 720);
    if (!m_browser->start(window, url, box, m_framesPerSecond)) {
        setState(m_state, QStringLiteral("browserFailed"));
        return;
    }
    m_browserSession = true;
    m_sharing = true;
    m_publishStarted = false;
    m_controllers.clear();
    m_controlAll = false;
    m_announce.start();
    emit controlChanged();
    setState(m_state);
    if (m_roomId.isEmpty())
        connectDemo();
}

void ScreenController::setControl(const QString &identity, bool allowed)
{
    if (!m_browserSession || !isUuid(identity) || identity.toLower() == m_room->viewerId())
        return;
    const QString id = identity.toLower();
    if (allowed && !m_controllers.contains(id))
        m_controllers.append(id);
    else if (!allowed)
        m_controllers.removeAll(id);
    emit controlChanged();
    announceControl();
    syncChannel();
}

void ScreenController::setControlAll(bool allowed)
{
    if (!m_browserSession)
        return;
    m_controlAll = allowed;
    if (!allowed)
        m_controllers.clear();
    emit controlChanged();
    announceControl();
    syncChannel();
}

void ScreenController::syncChannel()
{
    // Members who are not hosts may send data only while a host opened the
    // demo channel; open it while any of them has control, close it after.
    bool needed = m_controlAll;
    for (const QVariant &member : m_room->members()) {
        const QVariantMap entry = member.toMap();
        const QString role = entry.value(QStringLiteral("role")).toString();
        if (m_controllers.contains(entry.value(QStringLiteral("id")).toString().toLower())
            && role != QLatin1StringView("owner") && role != QLatin1StringView("moderator"))
            needed = true;
    }
    if (m_room->canControl() && needed != m_room->demoChannelOpen())
        post(QStringLiteral("/annotations"), QJsonObject{{QStringLiteral("open"), needed}});
}

void ScreenController::announceControl()
{
    if (!m_browserSession || !m_connected)
        return;
    QJsonArray ids;
    for (const QString &id : m_controllers)
        ids.append(id);
    const QJsonObject message{{"t", "control"}, {"all", m_controlAll}, {"ids", ids}};
    m_engine->publishData(QJsonDocument(message).toJson(QJsonDocument::Compact), true, {});
}

void ScreenController::sendInput(const QVariantMap &event)
{
    if (!m_canControl || m_sharing || m_room->demoUserId().isEmpty())
        return;
    QJsonObject message = QJsonObject::fromVariantMap(event);
    message.insert(QStringLiteral("t"), QStringLiteral("input"));
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    // Pointer moves may be lost; clicks, wheel and keys must arrive.
    const bool reliable = !(event.value(QStringLiteral("type")) == QLatin1StringView("mouse")
                            && event.value(QStringLiteral("action")) == QLatin1StringView("move"));
    m_engine->publishData(payload, reliable, m_room->demoUserId());
}

void ScreenController::onData(const QString &identity, const QByteArray &payload)
{
    const QJsonObject message = QJsonDocument::fromJson(payload).object();
    const QString kind = message.value(QLatin1StringView("t")).toString();
    const QString sender = identity.toLower();
    if (kind == QLatin1StringView("control")) {
        // Only the presenter says who may control.
        if (m_sharing || sender != m_room->demoUserId())
            return;
        const QString me = m_room->viewerId();
        bool allowed = message.value(QLatin1StringView("all")).toBool();
        for (const QJsonValue &id : message.value(QLatin1StringView("ids")).toArray())
            allowed = allowed || id.toString() == me;
        if (allowed != m_canControl) {
            m_canControl = allowed;
            emit controlChanged();
        }
        return;
    }
    if (kind != QLatin1StringView("input") || !m_browserSession || !m_browser)
        return;
    if (!m_controlAll && !m_controllers.contains(sender))
        return;
    // At most 80 messages a second from one member.
    const qint64 second = m_clock.elapsed() / 1000;
    auto &rate = m_rate[sender];
    if (rate.first != second)
        rate = {second, 0};
    if (++rate.second > 80)
        return;
    m_browser->input(message);
}

void ScreenController::setShareAudio(bool value)
{
    if (value == m_shareAudio)
        return;
    m_shareAudio = value;
    emit audioChanged();
    if (m_sharing && m_publishStarted)
        value ? startSound() : stopSound();
}

void ScreenController::setRemoteAudioMuted(bool value)
{
    if (value == m_remoteAudioMuted)
        return;
    m_remoteAudioMuted = value;
    m_engine->setRemoteAudioMuted(value);
    emit audioChanged();
}

void ScreenController::startSound()
{
    if (!m_shareAudio || m_audio.running())
        return;
    // The browser's or the window's application, with its child processes;
    // for a whole screen, everything but Katsodi (voice, room video, player).
    quint32 process = 0;
    bool include = true;
    if (m_browserSession && m_browser) {
        process = m_browser->processId();
    } else if (m_shareProcess) {
        process = m_shareProcess;
    } else {
        process = quint32(QCoreApplication::applicationPid());
        include = false;
    }
    ScreenEngine *engine = m_engine;
    const bool started = process && m_audio.start(process, include, [engine](std::vector<std::int16_t> &&chunk) {
        engine->pushAudio(std::move(chunk));
    });
    if (started)
        m_engine->startAudio();
    m_audioState = started ? (include ? QStringLiteral("window") : QStringLiteral("system")) : QStringLiteral("unavailable");
    emit audioChanged();
}

void ScreenController::stopSound()
{
    if (m_audio.running()) {
        m_audio.stop();
        m_engine->stopAudio();
    }
    if (m_audioState != QLatin1StringView("off")) {
        m_audioState = QStringLiteral("off");
        emit audioChanged();
    }
}

} // namespace katsodi
