#include "media/ShareAudioPlayer.h"

#include <QAudioFormat>
#include <QMediaDevices>
#include <QMetaObject>

#include <cstring>

namespace katsodi {

/** A byte ring the audio sink pulls from; writers may be any thread. */
class ShareAudioPlayer::Ring : public QIODevice {
public:
    explicit Ring(qsizetype capacity) : m_capacity(capacity) { open(QIODevice::ReadOnly); }
    void push(const char *data, qsizetype size)
    {
        QMutexLocker lock(&m_mutex);
        m_data.append(data, size);
        if (m_data.size() > m_capacity)
            m_data.remove(0, m_data.size() - m_capacity); // drop the oldest
    }
    void clear()
    {
        QMutexLocker lock(&m_mutex);
        m_data.clear();
    }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 4096 + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *out, qint64 max) override
    {
        QMutexLocker lock(&m_mutex);
        const qint64 take = qMin<qint64>(max, m_data.size());
        std::memcpy(out, m_data.constData(), size_t(take));
        m_data.remove(0, take);
        // Run dry: silence, so the sink keeps going instead of idling.
        if (take < max)
            std::memset(out + take, 0, size_t(max - take));
        return max;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QMutex m_mutex;
    QByteArray m_data;
    qsizetype m_capacity;
};

ShareAudioPlayer::ShareAudioPlayer(QObject *parent) : QObject(parent) {}

ShareAudioPlayer::~ShareAudioPlayer() { stop(); }

void ShareAudioPlayer::write(const std::int16_t *samples, int frames, int sampleRate, int channels)
{
    if (!samples || frames <= 0 || sampleRate < 8000 || sampleRate > 192000 || channels < 1 || channels > 2)
        return;
    // One lock covers the ring pointer too: stop() may replace it meanwhile.
    QMutexLocker lock(&m_formatMutex);
    if (m_rate != sampleRate || m_channels != channels) {
        if (!m_starting) {
            m_starting = true;
            QMetaObject::invokeMethod(this, [this, sampleRate, channels] { begin(sampleRate, channels); },
                                      Qt::QueuedConnection);
        }
        return; // the first packets of a new format are dropped
    }
    if (m_ring)
        m_ring->push(reinterpret_cast<const char *>(samples), qsizetype(frames) * channels * 2);
}

void ShareAudioPlayer::begin(int sampleRate, int channels)
{
    stop();
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);
    // About half a second of sound at most.
    auto ring = std::make_unique<Ring>(qsizetype(sampleRate) * channels * 2 / 2);
    auto sink = std::make_unique<QAudioSink>(QMediaDevices::defaultAudioOutput(), format);
    sink->setBufferSize(sampleRate * channels * 2 / 10); // 100 ms
    sink->setVolume(m_muted ? 0.0 : m_volume);
    sink->start(ring.get());
    QMutexLocker lock(&m_formatMutex);
    m_ring = std::move(ring);
    m_sink = std::move(sink);
    m_rate = sampleRate;
    m_channels = channels;
    m_starting = false;
}

void ShareAudioPlayer::stop()
{
    std::unique_ptr<QAudioSink> sink;
    std::unique_ptr<Ring> ring;
    {
        QMutexLocker lock(&m_formatMutex);
        m_rate = m_channels = 0;
        m_starting = false;
        sink = std::move(m_sink);
        ring = std::move(m_ring);
    }
    // The sink stops pulling before its ring goes away.
    if (sink)
        sink->stop();
    sink.reset();
    ring.reset();
}

void ShareAudioPlayer::setVolume(qreal volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    if (m_sink)
        m_sink->setVolume(m_muted ? 0.0 : m_volume);
}

void ShareAudioPlayer::setMuted(bool muted)
{
    m_muted = muted;
    if (m_sink)
        m_sink->setVolume(m_muted ? 0.0 : m_volume);
}

} // namespace katsodi
