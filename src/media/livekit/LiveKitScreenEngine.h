#pragma once
#include "media/ScreenEngine.h"
#include "media/ShareAudioPlayer.h"

#include <QMutex>
#include <QThread>
#include <atomic>
#include <memory>

class QVideoFrame;

namespace katsodi {

/**
 * Link demos through the official LiveKit C++ SDK.
 *
 * Threading, as in LiveKitVoiceEngine: every SDK call runs on one private
 * worker thread. Captured frames are handed over as QImages; one in flight
 * at most (a newer frame replaces a waiting one), so a slow encoder drops
 * frames instead of queueing them. The received screen is read on its own
 * thread (VideoStream::read blocks) into QVideoFrames delivered to the sink
 * on the sink's thread, again one in flight at most.
 */
class LiveKitScreenEngine final : public ScreenEngine {
    Q_OBJECT
public:
    explicit LiveKitScreenEngine(QObject *parent = nullptr);
    ~LiveKitScreenEngine() override;

    bool available() const override { return true; }
    void connectRoom(quint64 generation, const QString &url, const QString &token) override;
    void disconnectRoom() override;
    void startPublishing(QSize size, int framesPerSecond, int maxBitrate) override;
    void pushFrame(const QImage &frame) override;
    void setPaused(bool paused) override;
    void stopPublishing() override;
    void setVideoSink(QVideoSink *sink) override;
    void publishData(const QByteArray &payload, bool reliable, const QString &destination) override;
    void startAudio() override;
    void pushAudio(std::vector<std::int16_t> &&chunk) override;
    void stopAudio() override;
    void setRemoteAudioMuted(bool muted) override;
    ShareAudioPlayer *player() { return &m_player; }

    struct Session; // SDK objects of one connection, owned by the worker thread

    /** Delivers a received frame to the sink, dropping it if one is still pending. */
    void deliver(const QVideoFrame &frame);

private:
    void onWorker(std::function<void()> work);
    void endSession();
    void encode(Session &session, const QImage &image);
    void repeatPicture();

    QThread m_thread;
    QObject *m_worker = nullptr;
    std::shared_ptr<Session> m_session; // worker thread only
    std::atomic<quint64> m_active{0};

    QMutex m_frameMutex;
    QImage m_pendingFrame; // the newest captured frame not yet encoded
    std::atomic<bool> m_frameQueued{false};

    ShareAudioPlayer m_player; // the presenter's sound, for viewers
    std::atomic<int> m_audioQueued{0};
    QMutex m_sinkMutex;
    QPointer<QVideoSink> m_sink;
    // Shared with queued deliveries, which may run after this engine is gone.
    std::shared_ptr<std::atomic<bool>> m_sinkBusy = std::make_shared<std::atomic<bool>>(false);
};

} // namespace katsodi
