#include "media/livekit/LiveKitScreenEngine.h"
#include "media/livekit/LiveKitSdk.h"

#include <livekit/livekit.h>
#include <livekit/local_video_track.h>
#include <livekit/remote_participant.h>
#include <livekit/remote_track_publication.h>
#include <livekit/video_frame.h>
#include <livekit/video_source.h>
#include <livekit/video_stream.h>
#include <livekit/audio_frame.h>
#include <livekit/audio_source.h>
#include <livekit/audio_stream.h>
#include <livekit/local_audio_track.h>

#include <QMetaObject>
#include <QPainter>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>
#include <QTimer>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

namespace katsodi {

namespace {
constexpr const char *kScreenTrack = "demo";
constexpr const char *kAudioTrack = "demo-audio";
// The website's drawing uses topic "demo"; the shared browser has its own.
constexpr const char *kBrowserTopic = "katsodi-cobrowse";
constexpr std::size_t kMaxMessage = 4096;
} // namespace

/** One connection to the demo SFU room. */
struct LiveKitScreenEngine::Session : livekit::RoomDelegate {
    LiveKitScreenEngine *engine = nullptr;
    quint64 generation = 0;
    std::unique_ptr<livekit::Room> room;

    // Publishing (worker thread).
    std::shared_ptr<livekit::VideoSource> source;
    std::shared_ptr<livekit::LocalVideoTrack> track;
    QSize size;
    bool paused = false;
    // Captures deliver frames only on change: the last picture is repeated
    // so a still page reaches viewers and late joiners; a frame that came
    // before the track was ready is sent once it is.
    std::vector<std::uint8_t> lastPicture;
    std::chrono::steady_clock::time_point lastSent{};
    QImage early;
    qint64 started = 0;

    // The shared sound (worker thread).
    std::shared_ptr<livekit::AudioSource> audioSource;
    std::shared_ptr<livekit::LocalAudioTrack> audioTrack;

    // Receiving (SDK callback threads + the reader threads).
    std::string audioSid;
    std::shared_ptr<livekit::AudioStream> audioStream;
    std::thread audioReader;

    void stopListening()
    {
        std::shared_ptr<livekit::AudioStream> current;
        std::thread thread;
        {
            std::lock_guard lock(mutex);
            current = std::move(audioStream);
            thread = std::move(audioReader);
            audioSid.clear();
        }
        if (current)
            current->close();
        if (thread.joinable())
            thread.join();
    }

    void listen(const std::shared_ptr<livekit::Track> &track, const std::string &sid)
    {
        stopListening();
        livekit::AudioStream::Options options;
        options.capacity = 50; // about a second of 20 ms frames at most
        auto created = livekit::AudioStream::fromTrack(track, options);
        std::lock_guard lock(mutex);
        if (closing)
            return;
        audioStream = created;
        audioSid = sid;
        audioReader = std::thread([this, created] {
            livekit::AudioFrameEvent event;
            bool announced = false;
            while (created->read(event)) {
                const auto &frame = event.frame;
                if (frame.samplesPerChannel() <= 0 || frame.data().size()
                    < std::size_t(frame.samplesPerChannel()) * std::size_t(frame.numChannels()))
                    continue;
                if (!announced) {
                    announced = true;
                    emit engine->remoteAudioChanged(true);
                }
                engine->player()->write(frame.data().data(), frame.samplesPerChannel(), frame.sampleRate(),
                                        frame.numChannels());
            }
            emit engine->remoteAudioChanged(false);
        });
    }

    // Receiving (SDK callback threads + the reader thread).
    std::mutex mutex;
    bool closing = false;
    std::string watchedSid;
    std::shared_ptr<livekit::VideoStream> stream;
    std::thread reader;

    void stopWatching()
    {
        std::shared_ptr<livekit::VideoStream> current;
        std::thread thread;
        {
            std::lock_guard lock(mutex);
            current = std::move(stream);
            thread = std::move(reader);
            watchedSid.clear();
        }
        if (current)
            current->close(); // wakes the blocking read()
        if (thread.joinable())
            thread.join();
    }

    void watch(const std::shared_ptr<livekit::Track> &screen, const std::string &sid)
    {
        stopWatching();
        livekit::VideoStream::Options options;
        options.capacity = 2; // never more than two decoded frames waiting
        options.format = livekit::VideoBufferType::BGRA;
        auto created = livekit::VideoStream::fromTrack(screen, options);
        std::lock_guard lock(mutex);
        if (closing)
            return;
        stream = created;
        watchedSid = sid;
        reader = std::thread([this, created] {
            livekit::VideoFrameEvent event;
            bool announced = false;
            while (created->read(event)) {
                const int w = event.frame.width(), h = event.frame.height();
                if (w <= 0 || h <= 0 || w > 8192 || h > 8192
                    || event.frame.dataSize() < std::size_t(w) * std::size_t(h) * 4)
                    continue;
                if (!announced) {
                    announced = true;
                    emit engine->remoteScreenChanged(true, QSize(w, h));
                }
                QVideoFrame frame(QVideoFrameFormat(QSize(w, h), QVideoFrameFormat::Format_BGRA8888));
                if (!frame.map(QVideoFrame::WriteOnly))
                    continue;
                const int stride = frame.bytesPerLine(0);
                const uchar *from = event.frame.data();
                uchar *to = frame.bits(0);
                for (int row = 0; row < h; ++row)
                    std::memcpy(to + qsizetype(row) * stride, from + qsizetype(row) * w * 4, size_t(w) * 4);
                frame.unmap();
                engine->deliver(frame);
            }
            emit engine->remoteScreenChanged(false, {});
        });
    }

    // --- RoomDelegate (SDK threads) ---------------------------------------
    void onTrackSubscribed(livekit::Room &, const livekit::TrackSubscribedEvent &event) override
    {
        if (event.track && event.publication && event.publication->kind() == livekit::TrackKind::KIND_AUDIO
            && event.publication->source() == livekit::TrackSource::SOURCE_SCREENSHARE_AUDIO) {
            {
                std::lock_guard lock(mutex);
                if (closing)
                    return;
            }
            listen(event.track, event.publication->sid());
            return;
        }
        if (!event.track || !event.publication || event.publication->kind() != livekit::TrackKind::KIND_VIDEO
            || event.publication->source() != livekit::TrackSource::SOURCE_SCREENSHARE)
            return;
        {
            std::lock_guard lock(mutex);
            if (closing)
                return;
        }
        watch(event.track, event.publication->sid());
    }
    void onTrackUnsubscribed(livekit::Room &, const livekit::TrackUnsubscribedEvent &event) override
    {
        bool mine = false, sound = false;
        {
            std::lock_guard lock(mutex);
            mine = event.publication && event.publication->sid() == watchedSid;
            sound = event.publication && event.publication->sid() == audioSid;
        }
        if (mine)
            stopWatching();
        if (sound)
            stopListening();
    }
    void onUserPacketReceived(livekit::Room &, const livekit::UserDataPacketEvent &event) override
    {
        if (!event.participant || event.topic != kBrowserTopic || event.data.empty() || event.data.size() > kMaxMessage)
            return;
        {
            std::lock_guard lock(mutex);
            if (closing)
                return;
        }
        emit engine->dataReceived(QString::fromStdString(event.participant->identity()),
                                  QByteArray(reinterpret_cast<const char *>(event.data.data()), qsizetype(event.data.size())));
    }
    void onDisconnected(livekit::Room &, const livekit::DisconnectedEvent &event) override
    {
        {
            std::lock_guard lock(mutex);
            if (closing)
                return;
        }
        using R = livekit::DisconnectReason;
        const Ended reason = event.reason == R::Unknown ? Ended::Network
                             : event.reason == R::ClientInitiated ? Ended::Client
                                                                  : Ended::Removed;
        emit engine->ended(generation, reason);
    }
};

LiveKitScreenEngine::LiveKitScreenEngine(QObject *parent) : ScreenEngine(parent)
{
    qRegisterMetaType<ScreenEngine::Ended>();
    m_thread.setObjectName(QStringLiteral("katsodi-screen"));
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
    onWorker([this] {
        livekitsdk::acquire();
        auto *repeat = new QTimer(m_worker); // lives and fires on the worker thread
        repeat->setInterval(500);
        connect(repeat, &QTimer::timeout, m_worker, [this] {
            try {
                repeatPicture();
            } catch (...) {
            }
        });
        repeat->start();
    });
}

LiveKitScreenEngine::~LiveKitScreenEngine()
{
    {
        QMutexLocker lock(&m_sinkMutex);
        m_sink.clear();
    }
    QMetaObject::invokeMethod(m_worker, [this] {
        endSession();
        livekitsdk::release();
    }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void LiveKitScreenEngine::onWorker(std::function<void()> work)
{
    QMetaObject::invokeMethod(m_worker, [work = std::move(work)] {
        try {
            work();
        } catch (const std::exception &error) {
            qWarning("screen: %s", error.what());
        } catch (...) {
            qWarning("screen: unknown error");
        }
    }, Qt::QueuedConnection);
}

void LiveKitScreenEngine::endSession()
{
    auto session = std::move(m_session);
    if (!session)
        return;
    {
        std::lock_guard lock(session->mutex);
        session->closing = true;
    }
    session->stopWatching();
    session->stopListening();
    QMetaObject::invokeMethod(&m_player, [player = &m_player] { player->stop(); }, Qt::QueuedConnection);
    try {
        if (session->room) {
            if (session->audioTrack && session->audioTrack->publication())
                if (auto local = session->room->localParticipant().lock())
                    local->unpublishTrack(session->audioTrack->publication()->sid());
            if (session->track && session->track->publication())
                if (auto local = session->room->localParticipant().lock())
                    local->unpublishTrack(session->track->publication()->sid());
            session->room->disconnect();
            session->room->setDelegate(nullptr);
        }
    } catch (...) {
    }
    session->audioTrack.reset();
    session->audioSource.reset();
    const bool wasPublishing = session->track != nullptr;
    session->track.reset();
    session->source.reset();
    session->room.reset();
    if (wasPublishing)
        emit publishingChanged(false);
}

void LiveKitScreenEngine::connectRoom(quint64 generation, const QString &url, const QString &token)
{
    m_active = generation;
    onWorker([this, generation, url = url.toStdString(), token = token.toStdString()] {
        endSession();
        if (m_active != generation)
            return;
        auto session = std::make_shared<Session>();
        session->engine = this;
        session->generation = generation;
        session->room = std::make_unique<livekit::Room>();
        session->room->setDelegate(session.get());
        livekit::RoomOptions options;
        options.auto_subscribe = true;
        options.dynacast = true;
        options.connect_timeout = std::chrono::milliseconds(15000);
        if (!session->room->connect(url, token, options) || m_active != generation) {
            session->room->setDelegate(nullptr);
            if (m_active == generation)
                emit connectFailed(generation);
            return;
        }
        m_session = session;
        emit connected(generation);
        // A screen published before we joined arrives as a subscription too.
    });
}

void LiveKitScreenEngine::disconnectRoom()
{
    m_active = 0;
    onWorker([this] { endSession(); });
}

void LiveKitScreenEngine::startPublishing(QSize size, int framesPerSecond, int maxBitrate)
{
    onWorker([this, size, framesPerSecond, maxBitrate] {
        auto session = m_session;
        if (!session || !session->room || session->track)
            return;
        try {
            session->size = size;
            session->source = std::make_shared<livekit::VideoSource>(size.width(), size.height());
            session->track = livekit::LocalVideoTrack::createLocalVideoTrack(kScreenTrack, session->source);
            livekit::TrackPublishOptions publish;
            publish.source = livekit::TrackSource::SOURCE_SCREENSHARE;
            livekit::VideoEncodingOptions encoding;
            encoding.max_bitrate = std::uint64_t(maxBitrate);
            encoding.max_framerate = framesPerSecond;
            publish.video_encoding = encoding;
            publish.simulcast = false; // one layer: screens are watched whole
            auto local = session->room->localParticipant().lock();
            if (!local)
                throw std::runtime_error("no local participant");
            local->publishTrack(session->track, publish);
            if (session->paused)
                session->track->mute();
            emit publishingChanged(true);
            if (!session->early.isNull())
                encode(*session, std::exchange(session->early, QImage()));
        } catch (const std::exception &error) {
            qWarning("screen: publish: %s", error.what());
            session->track.reset();
            session->source.reset();
            emit publishFailed();
        }
    });
}

void LiveKitScreenEngine::encode(Session &session, const QImage &image)
{
    // Fit into the published size, letterboxed, as 32-bit BGRA.
    const QSize target = session.size;
    QImage canvas(target, QImage::Format_RGB32);
    if (image.size() == target) {
        canvas = image.convertToFormat(QImage::Format_RGB32);
    } else {
        canvas.fill(Qt::black);
        const QImage scaled = image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                  .convertToFormat(QImage::Format_RGB32);
        QPainter painter(&canvas);
        painter.drawImage((target.width() - scaled.width()) / 2, (target.height() - scaled.height()) / 2, scaled);
    }
    const int rowBytes = target.width() * 4;
    std::vector<std::uint8_t> bytes(std::size_t(rowBytes) * std::size_t(target.height()));
    for (int row = 0; row < target.height(); ++row)
        std::memcpy(bytes.data() + qsizetype(row) * rowBytes, canvas.constScanLine(row), size_t(rowBytes));
    session.lastPicture = bytes;
    session.lastSent = std::chrono::steady_clock::now();
    session.source->captureFrame(
        livekit::VideoFrame(target.width(), target.height(), livekit::VideoBufferType::BGRA, std::move(bytes)));
}

void LiveKitScreenEngine::repeatPicture()
{
    auto session = m_session;
    if (!session || !session->source || session->paused || session->lastPicture.empty()
        || std::chrono::steady_clock::now() - session->lastSent < std::chrono::milliseconds(450))
        return;
    session->lastSent = std::chrono::steady_clock::now();
    session->source->captureFrame(livekit::VideoFrame(session->size.width(), session->size.height(),
                                                      livekit::VideoBufferType::BGRA, session->lastPicture));
}

void LiveKitScreenEngine::pushFrame(const QImage &frame)
{
    {
        QMutexLocker lock(&m_frameMutex);
        m_pendingFrame = frame; // a newer frame replaces one still waiting
    }
    if (m_frameQueued.exchange(true))
        return;
    onWorker([this] {
        QImage image;
        {
            QMutexLocker lock(&m_frameMutex);
            image = std::move(m_pendingFrame);
            m_pendingFrame = QImage();
        }
        m_frameQueued = false;
        auto session = m_session;
        if (!session || session->paused || image.isNull())
            return;
        if (!session->source) {
            session->early = image; // the track is not ready yet
            return;
        }
        encode(*session, image);
    });
}

void LiveKitScreenEngine::setPaused(bool paused)
{
    onWorker([this, paused] {
        auto session = m_session;
        if (!session)
            return;
        session->paused = paused;
        if (session->track)
            paused ? session->track->mute() : session->track->unmute();
        if (session->audioTrack)
            paused ? session->audioTrack->mute() : session->audioTrack->unmute();
    });
}

void LiveKitScreenEngine::stopPublishing()
{
    onWorker([this] {
        auto session = m_session;
        if (!session || !session->track)
            return;
        try {
            if (session->track->publication())
                if (auto local = session->room->localParticipant().lock())
                    local->unpublishTrack(session->track->publication()->sid());
        } catch (...) {
        }
        session->track.reset();
        session->source.reset();
        emit publishingChanged(false);
    });
}

void LiveKitScreenEngine::publishData(const QByteArray &payload, bool reliable, const QString &destination)
{
    if (payload.isEmpty() || payload.size() > qsizetype(kMaxMessage))
        return;
    onWorker([this, payload, reliable, destination = destination.toStdString()] {
        auto session = m_session;
        if (!session || !session->room)
            return;
        auto local = session->room->localParticipant().lock();
        if (!local)
            return;
        const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
        std::vector<std::string> to;
        if (!destination.empty())
            to.push_back(destination);
        local->publishData(bytes, reliable, to, kBrowserTopic);
    });
}

void LiveKitScreenEngine::startAudio()
{
    onWorker([this] {
        auto session = m_session;
        if (!session || !session->room || session->audioTrack)
            return;
        try {
            // A 200 ms queue absorbs the worker's pauses for video frames.
            session->audioSource = std::make_shared<livekit::AudioSource>(48000, 2, 200);
            session->audioTrack = livekit::LocalAudioTrack::createLocalAudioTrack(kAudioTrack, session->audioSource);
            livekit::TrackPublishOptions publish;
            publish.source = livekit::TrackSource::SOURCE_SCREENSHARE_AUDIO;
            publish.dtx = false; // music and films are not speech
            auto local = session->room->localParticipant().lock();
            if (!local)
                throw std::runtime_error("no local participant");
            local->publishTrack(session->audioTrack, publish);
            if (session->paused)
                session->audioTrack->mute();
        } catch (const std::exception &error) {
            qWarning("screen: audio: %s", error.what());
            session->audioTrack.reset();
            session->audioSource.reset();
        }
    });
}

void LiveKitScreenEngine::pushAudio(std::vector<std::int16_t> &&chunk)
{
    // Never more than ~200 ms waiting: a stalled worker drops sound, not time.
    if (chunk.size() != 960 * 2 || m_audioQueued.load() > 10)
        return;
    ++m_audioQueued;
    onWorker([this, chunk = std::move(chunk)]() mutable {
        --m_audioQueued;
        auto session = m_session;
        if (!session || !session->audioSource || session->paused)
            return;
        session->audioSource->captureFrame(livekit::AudioFrame(std::move(chunk), 48000, 2, 960), 0);
    });
}

void LiveKitScreenEngine::stopAudio()
{
    onWorker([this] {
        auto session = m_session;
        if (!session || !session->audioTrack)
            return;
        try {
            if (session->audioTrack->publication())
                if (auto local = session->room->localParticipant().lock())
                    local->unpublishTrack(session->audioTrack->publication()->sid());
        } catch (...) {
        }
        session->audioTrack.reset();
        session->audioSource.reset();
    });
}

void LiveKitScreenEngine::setRemoteAudioMuted(bool muted) { m_player.setMuted(muted); }

void LiveKitScreenEngine::setVideoSink(QVideoSink *sink)
{
    QMutexLocker lock(&m_sinkMutex);
    m_sink = sink;
}

void LiveKitScreenEngine::deliver(const QVideoFrame &frame)
{
    QPointer<QVideoSink> sink;
    {
        QMutexLocker lock(&m_sinkMutex);
        sink = m_sink;
    }
    if (!sink || m_sinkBusy->exchange(true))
        return; // the previous frame is still on its way: drop this one
    QMetaObject::invokeMethod(sink, [busy = m_sinkBusy, sink, frame] {
        if (sink)
            sink->setVideoFrame(frame);
        *busy = false;
    }, Qt::QueuedConnection);
}

} // namespace katsodi
