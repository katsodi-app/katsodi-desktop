#include "presentation/VoiceController.h"

#include "infrastructure/api/ApiClient.h"
#include "presentation/RoomController.h"

#include <QUrl>
#include <QVariantMap>

namespace katsodi {

VoiceController::VoiceController(ApiClient *api, RoomController *room, VoiceEngine *engine, QObject *parent)
    : QObject(parent), m_api(api), m_room(room), m_engine(engine)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, [this] { requestToken(true); });
    // Another room, a closed room or another account: the call ends.
    connect(room, &RoomController::roomChanged, this, [this] {
        if (m_state != QLatin1StringView("idle") && m_room->roomId() != m_callRoom)
            leave();
    });
    connect(room, &RoomController::stateChanged, this, [this] {
        if (m_state != QLatin1StringView("idle") && (!m_room->joined() || m_room->closed())) {
            leave();
            m_errorKey = QStringLiteral("voiceAccess");
            emit changed();
        }
    });

    connect(engine, &VoiceEngine::connected, this, [this](quint64 generation) {
        if (generation != m_generation)
            return;
        m_retries = 0;
        setState(QStringLiteral("connected"));
    });
    connect(engine, &VoiceEngine::connectFailed, this, [this](quint64 generation) {
        if (generation != m_generation)
            return;
        if (m_state == QLatin1StringView("reconnecting"))
            scheduleRetry();
        else {
            m_engine->disconnectCall();
            setState(QStringLiteral("idle"), m_engine->available() ? QStringLiteral("voiceNetwork")
                                                                   : QStringLiteral("voiceUnsupported"));
        }
    });
    connect(engine, &VoiceEngine::reconnecting, this, [this](quint64 generation) {
        if (generation == m_generation)
            setState(QStringLiteral("reconnecting"));
    });
    connect(engine, &VoiceEngine::reconnected, this, [this](quint64 generation) {
        if (generation == m_generation)
            setState(QStringLiteral("connected"));
    });
    connect(engine, &VoiceEngine::ended, this, [this](quint64 generation, VoiceEngine::Ended reason) {
        if (generation != m_generation)
            return;
        m_peers.clear();
        emit peersChanged();
        m_microphone = false;
        m_deafened = false;
        if (reason == VoiceEngine::Ended::Network) {
            setState(QStringLiteral("reconnecting"));
            scheduleRetry();
            return;
        }
        m_engine->disconnectCall();
        ++m_generation;
        setState(QStringLiteral("idle"), QStringLiteral("voiceDisconnected"));
    });
    connect(engine, &VoiceEngine::peersChanged, this, [this](quint64 generation, const QList<VoicePeer> &peers) {
        if (generation != m_generation || peers == m_peers)
            return;
        m_peers = peers;
        emit peersChanged();
    });
    connect(engine, &VoiceEngine::microphoneChanged, this, [this](bool on) {
        m_microphone = on;
        emit changed();
    });
    connect(engine, &VoiceEngine::microphoneFailed, this, [this] {
        m_errorKey = QStringLiteral("voiceMicrophone");
        emit changed();
    });
    connect(engine, &VoiceEngine::devicesChanged, this,
            [this](const QList<AudioDevice> &inputs, const QList<AudioDevice> &outputs) {
        m_inputs = inputs;
        m_outputs = outputs;
        emit devicesChanged();
    });
}

void VoiceController::setState(const QString &state, const QString &errorKey)
{
    m_state = state;
    m_errorKey = errorKey;
    emit changed();
}

void VoiceController::join()
{
    if (m_state != QLatin1StringView("idle"))
        return;
    if (!m_engine->available()) {
        setState(QStringLiteral("idle"), QStringLiteral("voiceUnsupported"));
        return;
    }
    if (m_room->roomId().isEmpty() || !m_room->joined()) {
        setState(QStringLiteral("idle"), QStringLiteral("voiceJoinRoom"));
        return;
    }
    m_retries = 0;
    m_callRoom = m_room->roomId();
    setState(QStringLiteral("joining"));
    requestToken(false);
}

void VoiceController::requestToken(bool retrying)
{
    const quint64 generation = ++m_generation;
    m_microphone = false;
    m_deafened = false;
    const QString room = m_callRoom;
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/voice-token").arg(room), {}, this,
                [this, generation, retrying](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation)
            return;
        if (error) {
            // Access refused (not a member, closed, revoked): stop at once.
            const bool access = error->status == 401 || error->status == 403 || error->status == 404;
            if (retrying && !access) {
                scheduleRetry();
                return;
            }
            m_engine->disconnectCall();
            setState(QStringLiteral("idle"), access ? QStringLiteral("voiceAccess")
                                             : error->code == QLatin1StringView("not_configured")
                                                 ? QStringLiteral("voiceUnavailable")
                                                 : QStringLiteral("voiceNetwork"));
            return;
        }
        const QString url = body.value(QLatin1StringView("url")).toString();
        const QString token = body.value(QLatin1StringView("token")).toString();
        const QUrl parsed(url);
        if (parsed.scheme() != QLatin1StringView("wss") || parsed.host().isEmpty() || token.isEmpty()
            || token.size() > 8192) {
            setState(QStringLiteral("idle"), QStringLiteral("voiceNetwork"));
            return;
        }
        m_engine->connectCall(generation, url, token);
    });
}

void VoiceController::scheduleRetry()
{
    if (m_retries >= static_cast<int>(std::size(retryDelaysMs))) {
        m_engine->disconnectCall();
        ++m_generation;
        setState(QStringLiteral("idle"), QStringLiteral("voiceNetwork"));
        return;
    }
    setState(QStringLiteral("reconnecting"));
    m_retry.start(retryDelaysMs[m_retries++]);
}

void VoiceController::leave()
{
    ++m_generation; // every late event of the old call is dropped
    m_retry.stop();
    m_engine->disconnectCall();
    m_peers.clear();
    m_microphone = false;
    m_deafened = false;
    m_callRoom.clear();
    emit peersChanged();
    setState(QStringLiteral("idle"));
}

void VoiceController::toggleMicrophone()
{
    if (m_state != QLatin1StringView("connected") || m_deafened)
        return;
    m_engine->setMicrophone(!m_microphone);
}

void VoiceController::toggleDeafen()
{
    if (m_state != QLatin1StringView("connected"))
        return;
    const bool next = !m_deafened;
    if (next) {
        m_restoreMicrophone = m_microphone;
        if (m_microphone)
            m_engine->setMicrophone(false);
    } else if (m_restoreMicrophone) {
        m_engine->setMicrophone(true);
    }
    m_engine->setDeafened(next);
    m_deafened = next;
    emit changed();
}

QVariantList VoiceController::peers() const
{
    QVariantList list;
    QHash<QString, QString> names;
    for (const QVariant &member : m_room->members()) {
        const QVariantMap map = member.toMap();
        names.insert(map.value(QStringLiteral("id")).toString(), map.value(QStringLiteral("name")).toString());
    }
    for (const VoicePeer &peer : m_peers) {
        const QString name = names.value(peer.identity, QStringLiteral("Katsodi"));
        list.append(QVariantMap{{QStringLiteral("id"), peer.identity},
                                {QStringLiteral("name"), name},
                                {QStringLiteral("initial"), name.left(1).toUpper()},
                                {QStringLiteral("self"), peer.local},
                                {QStringLiteral("microphone"), peer.local ? m_microphone && !m_deafened : peer.microphone},
                                {QStringLiteral("speaking"), peer.speaking}});
    }
    return list;
}

QVariantList VoiceController::toVariant(const QList<AudioDevice> &devices)
{
    QVariantList list;
    for (const AudioDevice &device : devices)
        list.append(QVariantMap{{QStringLiteral("id"), device.id}, {QStringLiteral("name"), device.name}});
    return list;
}

} // namespace katsodi
