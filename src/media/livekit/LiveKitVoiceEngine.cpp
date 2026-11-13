#include "media/livekit/LiveKitVoiceEngine.h"
#include "media/livekit/LiveKitSdk.h"

#include <livekit/livekit.h>
#include <livekit/platform_audio.h>
#include <livekit/remote_participant.h>
#include <livekit/remote_track_publication.h>

#include <QMetaObject>
#include <QSet>
#include <mutex>

namespace katsodi {

namespace {
constexpr const char *kMicrophoneTrack = "microphone";

} // namespace

/** One connection: SDK room, platform audio and the delegate. */
struct LiveKitVoiceEngine::Call : livekit::RoomDelegate {
    LiveKitVoiceEngine *engine = nullptr;
    quint64 generation = 0;
    std::unique_ptr<livekit::Room> room;
    std::shared_ptr<livekit::PlatformAudio> audio;
    std::shared_ptr<livekit::LocalAudioTrack> microphone;
    bool microphoneOn = false;
    bool deafened = false;

    std::mutex mutex; // guards the fields below (SDK callback threads)
    QSet<QString> speaking;
    bool closing = false;

    /** Current participants as plain data. Called on SDK or worker threads. */
    QList<VoicePeer> snapshot()
    {
        QList<VoicePeer> peers;
        if (!room)
            return peers;
        std::lock_guard lock(mutex);
        if (auto local = room->localParticipant().lock()) {
            VoicePeer self;
            self.identity = QString::fromStdString(local->identity());
            self.local = true;
            self.microphone = microphoneOn && !deafened;
            self.speaking = speaking.contains(self.identity);
            peers.append(self);
        }
        for (const auto &weak : room->remoteParticipants()) {
            const auto remote = weak.lock();
            if (!remote)
                continue;
            VoicePeer peer;
            peer.identity = QString::fromStdString(remote->identity());
            for (const auto &[sid, publication] : remote->trackPublications())
                if (publication && publication->kind() == livekit::TrackKind::KIND_AUDIO
                    && publication->source() == livekit::TrackSource::SOURCE_MICROPHONE && !publication->muted())
                    peer.microphone = true;
            peer.speaking = speaking.contains(peer.identity);
            peers.append(peer);
            if (peers.size() >= 100)
                break;
        }
        return peers;
    }

    void publish()
    {
        {
            std::lock_guard lock(mutex);
            if (closing)
                return;
        }
        emit engine->peersChanged(generation, snapshot());
    }

    // --- RoomDelegate (SDK threads) ---------------------------------------
    void onParticipantConnected(livekit::Room &, const livekit::ParticipantConnectedEvent &) override { publish(); }
    void onParticipantDisconnected(livekit::Room &, const livekit::ParticipantDisconnectedEvent &) override { publish(); }
    void onTrackMuted(livekit::Room &, const livekit::TrackMutedEvent &) override { publish(); }
    void onTrackUnmuted(livekit::Room &, const livekit::TrackUnmutedEvent &) override { publish(); }
    void onTrackPublished(livekit::Room &, const livekit::TrackPublishedEvent &) override { publish(); }
    void onTrackUnpublished(livekit::Room &, const livekit::TrackUnpublishedEvent &) override { publish(); }
    void onTrackSubscribed(livekit::Room &, const livekit::TrackSubscribedEvent &event) override
    {
        // While deafened, newly arriving audio is not received or played.
        if (deafened && event.publication && event.publication->kind() == livekit::TrackKind::KIND_AUDIO)
            event.publication->setEnabled(false);
        publish();
    }
    void onActiveSpeakersChanged(livekit::Room &, const livekit::ActiveSpeakersChangedEvent &event) override
    {
        {
            std::lock_guard lock(mutex);
            speaking.clear();
            for (const auto *participant : event.speakers)
                if (participant)
                    speaking.insert(QString::fromStdString(participant->identity()));
        }
        publish();
    }
    void onReconnecting(livekit::Room &, const livekit::ReconnectingEvent &) override
    {
        emit engine->reconnecting(generation);
    }
    void onReconnected(livekit::Room &, const livekit::ReconnectedEvent &) override
    {
        emit engine->reconnected(generation);
        publish();
    }
    void onDisconnected(livekit::Room &, const livekit::DisconnectedEvent &event) override
    {
        {
            std::lock_guard lock(mutex);
            if (closing)
                return; // our own leave
        }
        using R = livekit::DisconnectReason;
        // An unexplained drop may be retried; a removal (kick, closed room,
        // revoked access, duplicate identity) must not be.
        const Ended reason = event.reason == R::Unknown ? Ended::Network
                             : event.reason == R::ClientInitiated ? Ended::Client
                                                                  : Ended::Removed;
        emit engine->ended(generation, reason);
    }
};

LiveKitVoiceEngine::LiveKitVoiceEngine(QObject *parent) : VoiceEngine(parent)
{
    qRegisterMetaType<QList<VoicePeer>>();
    qRegisterMetaType<QList<AudioDevice>>();
    qRegisterMetaType<VoiceEngine::Ended>();
    m_thread.setObjectName(QStringLiteral("katsodi-voice"));
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
    onWorker([] { livekitsdk::acquire(); });
}

LiveKitVoiceEngine::~LiveKitVoiceEngine()
{
    // Leave synchronously so no microphone, speaker or socket outlives us.
    QMetaObject::invokeMethod(m_worker, [this] {
        endCall();
        livekitsdk::release();
    }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void LiveKitVoiceEngine::onWorker(std::function<void()> work)
{
    QMetaObject::invokeMethod(m_worker, [work = std::move(work)] {
        try {
            work();
        } catch (const std::exception &error) {
            // SDK exceptions never cross into Qt's event loop.
            qWarning("voice: %s", error.what());
        } catch (...) {
            qWarning("voice: unknown error");
        }
    }, Qt::QueuedConnection);
}

void LiveKitVoiceEngine::endCall()
{
    auto call = std::move(m_call);
    if (!call)
        return;
    {
        std::lock_guard lock(call->mutex);
        call->closing = true;
    }
    try {
        if (call->room) {
            if (call->microphone && call->microphone->publication())
                if (auto local = call->room->localParticipant().lock())
                    local->unpublishTrack(call->microphone->publication()->sid());
            call->room->disconnect();
            call->room->setDelegate(nullptr);
        }
    } catch (...) {
    }
    call->microphone.reset();
    call->room.reset();
    call->audio.reset(); // releases the audio device module
}

void LiveKitVoiceEngine::connectCall(quint64 generation, const QString &url, const QString &token)
{
    m_active = generation;
    onWorker([this, generation, url = url.toStdString(), token = token.toStdString()] {
        endCall();
        if (m_active != generation)
            return;
        auto call = std::make_shared<Call>();
        call->engine = this;
        call->generation = generation;
        try {
            call->audio = std::make_shared<livekit::PlatformAudio>();
        } catch (const livekit::PlatformAudioError &error) {
            qWarning("voice: no audio device module: %s", error.what());
            emit connectFailed(generation);
            return;
        }
        call->room = std::make_unique<livekit::Room>();
        call->room->setDelegate(call.get());
        livekit::RoomOptions options;
        options.auto_subscribe = true;
        options.dynacast = true;
        options.connect_timeout = std::chrono::milliseconds(15000);
        if (!call->room->connect(url, token, options) || m_active != generation) {
            call->room->setDelegate(nullptr);
            if (m_active == generation)
                emit connectFailed(generation);
            return;
        }
        m_call = call;
        emit connected(generation);
        call->publish();
    });
    refreshDevices();
}

void LiveKitVoiceEngine::setMicrophone(bool on)
{
    onWorker([this, on] {
        auto call = m_call;
        if (!call || !call->room)
            return;
        try {
            if (on && !call->microphone) {
                livekit::PlatformAudioOptions processing; // AEC, NS, AGC on
                auto source = call->audio->createAudioSource(processing);
                auto track = livekit::LocalAudioTrack::createLocalAudioTrack(kMicrophoneTrack, source);
                livekit::TrackPublishOptions publish;
                publish.source = livekit::TrackSource::SOURCE_MICROPHONE;
                publish.dtx = true; // send nothing during silence
                if (auto local = call->room->localParticipant().lock())
                    local->publishTrack(track, publish);
                call->microphone = track;
            } else if (call->microphone) {
                on ? call->microphone->unmute() : call->microphone->mute();
            }
            call->microphoneOn = on;
            emit microphoneChanged(on);
            call->publish();
        } catch (const std::exception &error) {
            qWarning("voice: microphone: %s", error.what());
            call->microphoneOn = false;
            emit microphoneChanged(false);
            emit microphoneFailed();
        }
    });
}

void LiveKitVoiceEngine::setDeafened(bool on)
{
    onWorker([this, on] {
        auto call = m_call;
        if (!call || !call->room)
            return;
        call->deafened = on;
        // Disabled publications are neither received nor played.
        for (const auto &weak : call->room->remoteParticipants())
            if (const auto remote = weak.lock())
                for (const auto &[sid, publication] : remote->trackPublications())
                    if (publication && publication->kind() == livekit::TrackKind::KIND_AUDIO)
                        publication->setEnabled(!on);
        call->publish();
    });
}

void LiveKitVoiceEngine::disconnectCall()
{
    m_active = 0;
    onWorker([this] { endCall(); });
}

void LiveKitVoiceEngine::refreshDevices()
{
    onWorker([this] {
        QList<AudioDevice> inputs, outputs;
        auto audio = m_call ? m_call->audio : std::shared_ptr<livekit::PlatformAudio>();
        if (!audio)
            return;
        for (const auto &device : audio->recordingDevices())
            inputs.append({QString::fromStdString(device.id), QString::fromStdString(device.name).left(120)});
        for (const auto &device : audio->playoutDevices())
            outputs.append({QString::fromStdString(device.id), QString::fromStdString(device.name).left(120)});
        emit devicesChanged(inputs, outputs);
    });
}

void LiveKitVoiceEngine::setInputDevice(const QString &id)
{
    onWorker([this, id = id.toStdString()] {
        if (m_call && m_call->audio && !id.empty())
            m_call->audio->setRecordingDevice(id);
    });
}

void LiveKitVoiceEngine::setOutputDevice(const QString &id)
{
    onWorker([this, id = id.toStdString()] {
        if (m_call && m_call->audio && !id.empty())
            m_call->audio->setPlayoutDevice(id);
    });
}

} // namespace katsodi
