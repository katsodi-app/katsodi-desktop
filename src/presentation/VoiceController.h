#pragma once
#include "media/VoiceEngine.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace katsodi {

class ApiClient;
class RoomController;

/**
 * The room's voice call, with the website's rules (voice-lounge.tsx):
 * explicit join, microphone off at first, mute, deafen (also mutes and
 * restores the microphone afterwards), device choice, speaking state.
 *
 * Tokens come from POST /api/native/v1/rooms/{id}/voice-token, which checks
 * membership and the revocation worker every time. An unexplained drop is
 * retried after 2, 5 and 10 s with a fresh token and the microphone off; a
 * removal or refused token is never retried. Leaving the room, switching
 * account or quitting ends the call.
 */
class VoiceController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY changed) // idle, joining, connected, reconnecting
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    Q_PROPERTY(bool microphone READ microphone NOTIFY changed)
    Q_PROPERTY(bool deafened READ deafened NOTIFY changed)
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY devicesChanged)
public:
    VoiceController(ApiClient *api, RoomController *room, VoiceEngine *engine, QObject *parent = nullptr);

    bool available() const { return m_engine->available(); }
    QString state() const { return m_state; }
    QString errorKey() const { return m_errorKey; }
    bool microphone() const { return m_microphone; }
    bool deafened() const { return m_deafened; }
    QVariantList peers() const;
    QVariantList inputs() const { return toVariant(m_inputs); }
    QVariantList outputs() const { return toVariant(m_outputs); }

    Q_INVOKABLE void join();
    Q_INVOKABLE void leave();
    Q_INVOKABLE void toggleMicrophone();
    Q_INVOKABLE void toggleDeafen();
    Q_INVOKABLE void setInput(const QString &id) { m_engine->setInputDevice(id); }
    Q_INVOKABLE void setOutput(const QString &id) { m_engine->setOutputDevice(id); }
    Q_INVOKABLE void refreshDevices() { m_engine->refreshDevices(); }

    static constexpr int retryDelaysMs[] = {2000, 5000, 10000};

signals:
    void changed();
    void peersChanged();
    void devicesChanged();

private:
    void requestToken(bool retrying);
    void setState(const QString &state, const QString &errorKey = {});
    void scheduleRetry();
    static QVariantList toVariant(const QList<AudioDevice> &devices);

    ApiClient *m_api;
    RoomController *m_room;
    VoiceEngine *m_engine;
    QString m_state = QStringLiteral("idle");
    QString m_errorKey;
    QString m_callRoom;
    bool m_microphone = false;
    bool m_deafened = false;
    bool m_restoreMicrophone = false;
    QList<VoicePeer> m_peers;
    QList<AudioDevice> m_inputs, m_outputs;
    quint64 m_generation = 0;
    int m_retries = 0;
    QTimer m_retry;
};

} // namespace katsodi
