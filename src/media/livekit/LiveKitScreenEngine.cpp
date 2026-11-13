#include "media/livekit/LiveKitScreenEngine.h"
#include "media/livekit/LiveKitSdk.h"

#include <livekit/livekit.h>
#include <livekit/local_video_track.h>
#include <livekit/remote_participant.h>
#include <livekit/remote_track_publication.h>
#include <livekit/video_frame.h>
#include <livekit/video_source.h>
#include <livekit/video_stream.h>

#include <QMetaObject>
#include <QPainter>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>
#include <cstring>
#include <mutex>
#include <thread>

namespace katsodi {

namespace {
constexpr const char *kScreenTrack = "demo";
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
    qint64 started = 0;

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
        bool mine = false;
        {
            std::lock_guard lock(mutex);
            mine = event.publication && event.publication->sid() == watchedSid;
        }
        if (mine)
            stopWatching();
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
    onWorker([] { livekitsdk::acquire(); });
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
    try {
        if (session->room) {
            if (session->track && session->track->publication())
                if (auto local = session->room->localParticipant().lock())
                    local->unpublishTrack(session->track->publication()->sid());
            session->room->disconnect();
            session->room->setDelegate(nullptr);
        }
    } catch (...) {
    }
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
        } catch (const std::exception &error) {
            qWarning("screen: publish: %s", error.what());
            session->track.reset();
            session->source.reset();
            emit publishFailed();
        }
    });
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
        if (!session || !session->source || session->paused || image.isNull())
            return;
        // Fit into the published size, letterboxed, as 32-bit BGRA.
        const QSize target = session->size;
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
        auto buffer = livekit::VideoFrame::create(target.width(), target.height(), livekit::VideoBufferType::BGRA);
        const int rowBytes = target.width() * 4;
        for (int row = 0; row < target.height(); ++row)
            std::memcpy(buffer.data() + qsizetype(row) * rowBytes, canvas.constScanLine(row), size_t(rowBytes));
        session->source->captureFrame(buffer);
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
