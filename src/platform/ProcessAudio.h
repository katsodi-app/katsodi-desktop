#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace katsodi {

/**
 * The sound one application makes, for a demo: Windows process loopback
 * capture (Windows 10 2004 or newer). Either a process tree is included
 * (a shared window's application, the shared browser) or excluded (a whole
 * screen: everything except Katsodi, so the room's voice and video do not
 * come back to the others).
 *
 * PCM arrives as 48 kHz stereo 16-bit, in 20 ms chunks (1920 values), on
 * the capture thread; silence is delivered as zeros so the stream keeps time.
 */
class ProcessAudio : public QObject {
    Q_OBJECT
public:
    static constexpr int sampleRate = 48000;
    static constexpr int channels = 2;
    static constexpr int chunkFrames = 960; // 20 ms

    using Sink = std::function<void(std::vector<std::int16_t> &&chunk)>;

    explicit ProcessAudio(QObject *parent = nullptr);
    ~ProcessAudio() override;

    /** Starts capturing; false when the system cannot (too old, no device). */
    bool start(quint32 processId, bool includeTree, Sink sink);
    void stop();
    bool running() const { return m_thread.joinable(); }

signals:
    /** Capture stopped by itself (device gone, process ended). */
    void failed();

private:
    void run(quint32 processId, bool includeTree, Sink sink);

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_started{0}; // 0 pending, 1 running, -1 failed
};

} // namespace katsodi
