#pragma once
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>

class QVideoSink;

namespace katsodi {

/**
 * The media side of a link demo (screen sharing), independent of the SDK:
 * one connection to the room's demo SFU room, at most one published screen
 * and the presenter's screen received for display. Every method returns at
 * once; results arrive as queued signals. `generation` identifies one
 * connection attempt, so stale events can be dropped.
 */
class ScreenEngine : public QObject {
    Q_OBJECT
public:
    enum class Ended { Network, Client, Removed };
    Q_ENUM(Ended)

    using QObject::QObject;
    virtual bool available() const = 0;
    virtual void connectRoom(quint64 generation, const QString &url, const QString &token) = 0;
    /** Leaves the demo room, stops publishing and releases decoded frames. */
    virtual void disconnectRoom() = 0;
    /** Publishes a screen-share track with a fixed size; frames follow. */
    virtual void startPublishing(QSize size, int framesPerSecond, int maxBitrate) = 0;
    /** One captured frame (any size: fitted into the published size). Dropped while busy. */
    virtual void pushFrame(const QImage &frame) = 0;
    /** A host's pause: the track stays published but sends nothing. */
    virtual void setPaused(bool paused) = 0;
    virtual void stopPublishing() = 0;
    /** Where the presenter's received screen is shown (may be null). */
    virtual void setVideoSink(QVideoSink *sink) = 0;

signals:
    void connected(quint64 generation);
    void connectFailed(quint64 generation);
    void ended(quint64 generation, katsodi::ScreenEngine::Ended reason);
    /** Whether a remote screen is being received. */
    void remoteScreenChanged(bool present, QSize size);
    void publishingChanged(bool publishing);
    void publishFailed();
};

/** Builds without the LiveKit SDK: screen sharing is shown as unavailable. */
class NullScreenEngine final : public ScreenEngine {
    Q_OBJECT
public:
    using ScreenEngine::ScreenEngine;
    bool available() const override { return false; }
    void connectRoom(quint64 generation, const QString &, const QString &) override { emit connectFailed(generation); }
    void disconnectRoom() override {}
    void startPublishing(QSize, int, int) override { emit publishFailed(); }
    void pushFrame(const QImage &) override {}
    void setPaused(bool) override {}
    void stopPublishing() override {}
    void setVideoSink(QVideoSink *) override {}
};

} // namespace katsodi
