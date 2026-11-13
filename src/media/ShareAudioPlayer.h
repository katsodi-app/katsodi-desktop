#pragma once

#include <QAudioSink>
#include <QIODevice>
#include <QMutex>
#include <QObject>

#include <cstdint>
#include <memory>

namespace katsodi {

/**
 * Plays the sound of a demo (a shared window's or browser's audio) on the
 * default output, through Qt Multimedia, separately from the voice call's
 * audio device. PCM is written from any thread into a small ring (at most
 * ~0.5 s, the oldest dropped); the audio thread reads silence when it runs
 * dry, so a late packet never stalls playback.
 */
class ShareAudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit ShareAudioPlayer(QObject *parent = nullptr);
    ~ShareAudioPlayer() override;

    /** Thread-safe: 16-bit interleaved PCM; the first call fixes the format. */
    void write(const std::int16_t *samples, int frames, int sampleRate, int channels);
    /** Stops playback and forgets the format. GUI thread. */
    void stop();
    void setVolume(qreal volume);
    void setMuted(bool muted);

private:
    class Ring;
    void begin(int sampleRate, int channels);

    std::unique_ptr<Ring> m_ring;
    std::unique_ptr<QAudioSink> m_sink;
    QMutex m_formatMutex;
    int m_rate = 0, m_channels = 0;
    bool m_starting = false;
    qreal m_volume = 1.0;
    bool m_muted = false;
};

} // namespace katsodi
