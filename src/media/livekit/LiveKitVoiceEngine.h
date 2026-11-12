#pragma once
#include "media/VoiceEngine.h"

#include <QThread>
#include <atomic>
#include <memory>

namespace katsodi {

/**
 * Voice through the official LiveKit C++ SDK (prebuilt 1.13.1, standard
 * build with the platform Audio Device Module: WebRTC echo cancellation,
 * noise suppression and gain control, speaker playout of subscribed audio).
 *
 * Threading: every SDK call (connect, publish, mute, disconnect) runs on one
 * private worker thread, never on the GUI thread. SDK callbacks arrive on
 * SDK threads; they are reduced to plain data (identities and flags) and
 * delivered as queued signals. Nothing from the SDK touches QML.
 *
 * The microphone track is created only when the user turns the microphone
 * on for the first time; joining starts listen-only, as on the website.
 */
class LiveKitVoiceEngine final : public VoiceEngine {
    Q_OBJECT
public:
    explicit LiveKitVoiceEngine(QObject *parent = nullptr);
    ~LiveKitVoiceEngine() override;

    bool available() const override { return true; }
    void connectCall(quint64 generation, const QString &url, const QString &token) override;
    void setMicrophone(bool on) override;
    void setDeafened(bool on) override;
    void disconnectCall() override;
    void refreshDevices() override;
    void setInputDevice(const QString &id) override;
    void setOutputDevice(const QString &id) override;

    struct Call; // SDK objects of one connection, owned by the worker thread

private:
    void onWorker(std::function<void()> work);
    void endCall();

    QThread m_thread;
    QObject *m_worker = nullptr;
    std::shared_ptr<Call> m_call; // touched only on the worker thread
    std::atomic<quint64> m_active{0};
};

} // namespace katsodi
