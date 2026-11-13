#include "presentation/PlayerController.h"

#include "presentation/Preferences.h"
#include "presentation/RoomController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QVariantMap>
#include <cmath>

namespace katsodi {
namespace {

constexpr double driftTolerance = 1.5; // seconds, DRIFT_TOLERANCE on the website
constexpr qsizetype maxEventLine = 16 * 1024;

} // namespace

PlayerController::PlayerController(RoomController *room, Preferences *preferences, QUrl origin, QString helperPath,
                                   QObject *parent)
    : QObject(parent), m_room(room), m_preferences(preferences), m_origin(std::move(origin)),
      m_helperPath(std::move(helperPath))
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(20000);
    connect(&m_idle, &QTimer::timeout, this, &PlayerController::stopHelper);
    connect(m_room, &RoomController::stateChanged, this, &PlayerController::roomChanged);
    connect(m_room, &RoomController::roomChanged, this, &PlayerController::roomChanged);
    // A host's change made while the previous one was in flight goes next.
    connect(m_room, &RoomController::sendChanged, this, [this]() {
        if (!m_room->steering() && !m_pendingState.isEmpty()) {
            const QString state = std::exchange(m_pendingState, {});
            publish(state, std::exchange(m_pendingPosition, -1));
        }
    });
}

PlayerController::~PlayerController() { shutdown(); }

double PlayerController::expectedPosition(const QString &state, double position, qint64 elapsedMs)
{
    if (!std::isfinite(position) || position < 0)
        return 0;
    return state == QLatin1StringView("playing") ? position + std::max<qint64>(0, elapsedMs) / 1000.0 : position;
}

void PlayerController::setState(const QString &state, const QString &errorKey)
{
    if (m_state == state && m_errorKey == errorKey)
        return;
    m_state = state;
    m_errorKey = errorKey;
    emit changed();
}

void PlayerController::setVolume(int value)
{
    value = std::clamp(value, 0, 100);
    if (value == m_volume)
        return;
    m_volume = value;
    emit volumeChanged();
    if (m_ready)
        send({{QStringLiteral("op"), QStringLiteral("volume")}, {QStringLiteral("value"), m_volume}});
}

void PlayerController::setObscured(bool value)
{
    if (value == m_obscured)
        return;
    m_obscured = value;
    emit changed();
    pushGeometry();
}

void PlayerController::setSurface(QWindow *window, qreal x, qreal y, qreal width, qreal height, bool visible)
{
    const QRect rect(qRound(x), qRound(y), qRound(width), qRound(height));
    const bool windowChanged = window != m_window;
    if (!windowChanged && rect == m_surface && visible == m_surfaceVisible)
        return;
    if (windowChanged && m_helper)
        stopHelper(); // the helper is a child of one window; start again for another
    m_window = window;
    m_surface = rect;
    m_surfaceVisible = visible && rect.width() > 0 && rect.height() > 0;
    if (m_surfaceVisible)
        m_wantedByStage = true;
    roomChanged();
    pushGeometry();
}

void PlayerController::pushGeometry()
{
    if (!m_helper || m_helper->state() != QProcess::Running)
        return;
    const bool show = m_surfaceVisible && !m_obscured && m_ready;
    if (show && m_surface != m_sentGeometry) {
        m_sentGeometry = m_surface;
        send({{QStringLiteral("op"), QStringLiteral("geometry")},
              {QStringLiteral("x"), m_surface.x()}, {QStringLiteral("y"), m_surface.y()},
              {QStringLiteral("w"), m_surface.width()}, {QStringLiteral("h"), m_surface.height()}});
    }
    if (show != m_sentVisible) {
        m_sentVisible = show;
        send({{QStringLiteral("op"), QStringLiteral("visible")}, {QStringLiteral("value"), show}});
    }
}

void PlayerController::roomChanged()
{
    const bool open = !m_room->roomId().isEmpty() && !m_room->closed();
    const QString source = open && !m_room->syncKind().isEmpty() ? m_room->source() : QString();
    if (source.isEmpty()) {
        m_wantedByStage = m_wantedByStage && open;
        if (!m_loadedSource.isEmpty()) {
            m_loadedSource.clear();
            m_ready = false;
            m_playing = false;
            m_position = m_duration = 0;
            send({{QStringLiteral("op"), QStringLiteral("unload")}});
            emit progress();
        }
        if (m_helper && !m_idle.isActive())
            m_idle.start();
        if (!m_helper)
            setState(QStringLiteral("off"));
        return;
    }
    m_idle.stop();
    if (!available() || !m_wantedByStage || !m_window)
        return;
    ensureHelper();
    if (source != m_loadedSource) {
        m_loadedSource = source;
        m_ready = false;
        m_playing = false;
        m_position = m_duration = 0;
        m_appliedStamp = -1;
        ++m_generation;
        setState(QStringLiteral("loading"));
        emit progress();
        pushGeometry();
        send({{QStringLiteral("op"), QStringLiteral("load")},
              {QStringLiteral("generation"), double(m_generation)},
              {QStringLiteral("kind"), m_room->syncKind()},
              {QStringLiteral("source"), source},
              {QStringLiteral("controls"), m_room->canControl()}});
    }
    // Every snapshot re-extrapolates the position to its server time, so the
    // local clock restarts with each new value, not only with a new change.
    if (m_room->playbackPosition() != m_serverPosition || !m_receivedAt.isValid()) {
        m_serverPosition = m_room->playbackPosition();
        m_receivedAt.start();
    }
    if (m_room->playbackStamp() != m_appliedStamp)
        applyServer();
}

void PlayerController::ensureHelper()
{
    if (m_helper)
        return;
    m_helper = new QProcess(this);
    m_helper->setProgram(m_helperPath);
    m_helper->setArguments({QStringLiteral("--origin"), m_origin.toString(QUrl::RemovePath | QUrl::StripTrailingSlash),
                            QStringLiteral("--parent"), QString::number(quintptr(m_window->winId()))});
    m_helper->setProcessChannelMode(QProcess::SeparateChannels);
    m_helper->setStandardErrorFile(QProcess::nullDevice());
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // Fewer renderer processes in Low mode.
    if (m_preferences->lowResources())
        environment.insert(QStringLiteral("KATSODI_PLAYER_LOW"), QStringLiteral("1"));
    m_helper->setProcessEnvironment(environment);
    connect(m_helper, &QProcess::readyReadStandardOutput, this, &PlayerController::readEvents);
    connect(m_helper, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        const bool expected = m_helper && m_helper->property("stopping").toBool();
        m_helper->deleteLater();
        m_helper = nullptr;
        m_ready = false;
        m_playing = false;
        m_loadedSource.clear();
        m_sentGeometry = {};
        m_sentVisible = false;
        emit progress();
        if (expected)
            setState(QStringLiteral("off"));
        else
            setState(QStringLiteral("error"), QStringLiteral("playerCrashed"));
    });
    connect(m_helper, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_helper->deleteLater();
            m_helper = nullptr;
            m_loadedSource.clear();
            setState(QStringLiteral("error"), QStringLiteral("playerUnavailable"));
        }
    });
    setState(QStringLiteral("starting"));
    m_helper->start();
}

void PlayerController::stopHelper()
{
    m_idle.stop();
    if (!m_helper)
        return;
    m_helper->setProperty("stopping", true);
    // Closing stdin is the helper's signal to quit; kill only if it hangs.
    m_helper->closeWriteChannel();
    QPointer<QProcess> helper = m_helper;
    QTimer::singleShot(3000, this, [helper]() {
        if (helper && helper->state() != QProcess::NotRunning)
            helper->kill();
    });
}

void PlayerController::shutdown()
{
    if (!m_helper)
        return;
    // Synchronous stop: the finished handler (which clears m_helper) must
    // not run in the middle of it.
    QProcess *helper = std::exchange(m_helper, nullptr);
    disconnect(helper, nullptr, this, nullptr);
    helper->closeWriteChannel();
    if (!helper->waitForFinished(2000))
        helper->kill();
    helper->waitForFinished(1000);
    delete helper;
}

void PlayerController::retry()
{
    if (m_state != QLatin1StringView("error"))
        return;
    setState(QStringLiteral("off"));
    m_loadedSource.clear();
    roomChanged();
}

void PlayerController::send(const QJsonObject &command)
{
    if (!m_helper || m_helper->state() == QProcess::NotRunning)
        return;
    m_helper->write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
}

void PlayerController::readEvents()
{
    m_buffer += m_helper->readAllStandardOutput();
    qsizetype end;
    while ((end = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(end);
        m_buffer.remove(0, end + 1);
        if (line.size() <= maxEventLine)
            handle(QJsonDocument::fromJson(line).object());
    }
    if (m_buffer.size() > maxEventLine)
        m_buffer.clear();
}

void PlayerController::handle(const QJsonObject &event)
{
    const QString kind = event.value(QLatin1StringView("ev")).toString();
    if (kind == QLatin1StringView("external")) {
        const QUrl url(event.value(QLatin1StringView("url")).toString());
        if (url.isValid() && url.scheme() == QLatin1StringView("https") && url.userInfo().isEmpty())
            emit externalRequested(url.toString());
        return;
    }
    if (kind == QLatin1StringView("error")) {
        setState(QStringLiteral("error"), QStringLiteral("playerFailed"));
        return;
    }
    if (kind != QLatin1StringView("state"))
        return;
    // Answers about an earlier source are dropped.
    if (qint64(event.value(QLatin1StringView("generation")).toDouble()) != m_generation)
        return;
    const QString error = event.value(QLatin1StringView("error")).toString();
    if (!error.isEmpty()) {
        m_ready = false;
        setState(QStringLiteral("error"), error == QLatin1StringView("embed_blocked") ? QStringLiteral("playerBlocked")
                                                                                      : QStringLiteral("playerFailed"));
        pushGeometry();
        return;
    }
    const bool wasReady = m_ready;
    m_ready = event.value(QLatin1StringView("ready")).toBool();
    const double position = event.value(QLatin1StringView("position")).toDouble();
    const double duration = event.value(QLatin1StringView("duration")).toDouble();
    const bool playing = event.value(QLatin1StringView("playing")).toBool();
    // A jump of the local position that time does not explain is a seek.
    const double elapsed = m_sinceLastPosition.isValid() ? m_sinceLastPosition.elapsed() / 1000.0 : 0;
    const bool jumped = m_ready && wasReady && std::abs(position - (m_lastPosition + (m_playing ? elapsed : 0))) > 2.5;
    m_sinceLastPosition.start();
    m_lastPosition = position;
    const bool changedPlaying = playing != m_playing;
    m_playing = playing;
    m_position = std::isfinite(position) && position >= 0 ? position : 0;
    m_duration = std::isfinite(duration) && duration >= 0 ? duration : 0;
    emit progress();
    if (m_ready && !wasReady) {
        setState(QStringLiteral("ready"));
        send({{QStringLiteral("op"), QStringLiteral("volume")}, {QStringLiteral("value"), m_volume}});
        pushGeometry();
        m_appliedStamp = -1;
        applyServer();
        return;
    }
    QStringList events;
    for (const QJsonValue &value : event.value(QLatin1StringView("events")).toArray())
        events.append(value.toString());
    if (events.contains(QLatin1StringView("ended")) && m_room->canControl()) {
        const QVariantList queue = m_room->queue();
        if (!queue.isEmpty())
            m_room->playFromQueue(queue.first().toMap().value(QStringLiteral("id")).toString());
        return;
    }
    if (suppressed())
        return;
    if (m_room->canControl()) {
        // The host's own actions in the provider's controls become the room's.
        if (changedPlaying || jumped)
            publish(m_playing ? QStringLiteral("playing") : QStringLiteral("paused"), m_position);
        return;
    }
    // Followers: correct drift while both play, at most every 3 s.
    if (m_room->playback() == QLatin1StringView("playing") && m_playing
        && (!m_sinceDriftFix.isValid() || m_sinceDriftFix.elapsed() > 3000)) {
        const double expected = expectedPosition(m_room->playback(), m_room->playbackPosition(),
                                                 m_receivedAt.isValid() ? m_receivedAt.elapsed() : 0);
        if (std::abs(expected - m_position) > driftTolerance) {
            m_sinceDriftFix.start();
            m_suppress.start();
            send({{QStringLiteral("op"), QStringLiteral("seek")}, {QStringLiteral("position"), expected}});
        }
    }
}

void PlayerController::applyServer()
{
    if (!m_ready)
        return;
    m_appliedStamp = m_room->playbackStamp();
    const QString state = m_room->playback();
    if (state.isEmpty())
        return; // no shared timeline yet: independent until a host starts one
    const double expected = expectedPosition(state, m_room->playbackPosition(),
                                             m_receivedAt.isValid() ? m_receivedAt.elapsed() : 0);
    m_suppress.start();
    if (std::abs(expected - m_position) > driftTolerance)
        send({{QStringLiteral("op"), QStringLiteral("seek")}, {QStringLiteral("position"), expected}});
    send({{QStringLiteral("op"), state == QLatin1StringView("playing") ? QStringLiteral("play") : QStringLiteral("pause")}});
}

void PlayerController::publish(const QString &state, double position)
{
    if (!m_room->canControl())
        return;
    if (m_room->steering()) {
        m_pendingState = state;
        m_pendingPosition = position;
        return;
    }
    m_room->setPlayback(state, std::max(0.0, position));
}

void PlayerController::togglePlay()
{
    if (!m_ready)
        return;
    const bool play = !m_playing;
    if (m_room->canControl()) {
        // Apply locally at once; the published state comes back as an echo.
        m_suppress.start();
        publish(play ? QStringLiteral("playing") : QStringLiteral("paused"), m_position);
    }
    send({{QStringLiteral("op"), play ? QStringLiteral("play") : QStringLiteral("pause")}});
}

void PlayerController::seek(double seconds)
{
    if (!m_ready || !m_room->canControl() || !std::isfinite(seconds))
        return;
    seconds = std::clamp(seconds, 0.0, m_duration > 0 ? m_duration : 172800.0);
    m_suppress.start();
    send({{QStringLiteral("op"), QStringLiteral("seek")}, {QStringLiteral("position"), seconds}});
    m_position = seconds;
    m_lastPosition = seconds;
    emit progress();
    publish(m_playing ? QStringLiteral("playing") : QStringLiteral("paused"), seconds);
}

} // namespace katsodi
