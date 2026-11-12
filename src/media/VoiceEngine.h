#pragma once
#include <QList>
#include <QObject>
#include <QString>

namespace katsodi {

/** A participant of the voice call, as plain data for the UI. */
struct VoicePeer {
    QString identity; // the user's id (server-issued token identity)
    bool local = false;
    bool microphone = false; // publishing an unmuted microphone
    bool speaking = false;
    bool operator==(const VoicePeer &) const = default;
};

struct AudioDevice {
    QString id;
    QString name;
    bool operator==(const AudioDevice &) const = default;
};

/**
 * The media side of a voice call, independent of the SDK. Every method
 * returns at once; results arrive as signals on the object's thread.
 * `generation` identifies one connection attempt, so the caller can drop
 * events of an attempt it has already abandoned.
 */
class VoiceEngine : public QObject {
    Q_OBJECT
public:
    enum class Ended { Network, Client, Removed };
    Q_ENUM(Ended)

    using QObject::QObject;
    /** False when this build has no voice SDK. */
    virtual bool available() const = 0;
    virtual void connectCall(quint64 generation, const QString &url, const QString &token) = 0;
    virtual void setMicrophone(bool on) = 0;
    virtual void setDeafened(bool on) = 0;
    /** Ends the call and releases microphone, speakers and network. */
    virtual void disconnectCall() = 0;
    virtual void refreshDevices() = 0;
    virtual void setInputDevice(const QString &id) = 0;
    virtual void setOutputDevice(const QString &id) = 0;

signals:
    void connected(quint64 generation);
    void connectFailed(quint64 generation);
    void reconnecting(quint64 generation);
    void reconnected(quint64 generation);
    void ended(quint64 generation, katsodi::VoiceEngine::Ended reason);
    void peersChanged(quint64 generation, const QList<katsodi::VoicePeer> &peers);
    void microphoneChanged(bool on);
    void microphoneFailed();
    void devicesChanged(const QList<katsodi::AudioDevice> &inputs, const QList<katsodi::AudioDevice> &outputs);
};

/** Builds without the LiveKit SDK: voice is shown as unavailable. */
class NullVoiceEngine final : public VoiceEngine {
    Q_OBJECT
public:
    using VoiceEngine::VoiceEngine;
    bool available() const override { return false; }
    void connectCall(quint64 generation, const QString &, const QString &) override { emit connectFailed(generation); }
    void setMicrophone(bool) override {}
    void setDeafened(bool) override {}
    void disconnectCall() override {}
    void refreshDevices() override {}
    void setInputDevice(const QString &) override {}
    void setOutputDevice(const QString &) override {}
};

} // namespace katsodi

Q_DECLARE_METATYPE(katsodi::VoicePeer)
Q_DECLARE_METATYPE(katsodi::AudioDevice)
