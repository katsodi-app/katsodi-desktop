#pragma once
#include "domain/Records.h"
#include "presentation/ChatModel.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace katsodi {

class ApiClient;
class Session;
class Preferences;

/**
 * Owns one open shared room: the authorized snapshot (polled with message
 * deltas), chat, members, joining, sending and the occupancy ping. Opening
 * another room or closing it cancels all of the previous room's work; late
 * answers carry a generation number and are dropped.
 */
class RoomController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString roomId READ roomId NOTIFY roomChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY stateChanged)
    Q_PROPERTY(QString name READ name NOTIFY stateChanged)
    Q_PROPERTY(QString description READ description NOTIFY stateChanged)
    Q_PROPERTY(QString format READ format NOTIFY stateChanged)
    Q_PROPERTY(QString visibility READ visibility NOTIFY stateChanged)
    Q_PROPERTY(QString source READ source NOTIFY stateChanged)
    Q_PROPERTY(QString fandomId READ fandomId NOTIFY stateChanged)
    Q_PROPERTY(bool joined READ joined NOTIFY stateChanged)
    Q_PROPERTY(bool closed READ closed NOTIFY stateChanged)
    Q_PROPERTY(QString role READ role NOTIFY stateChanged)
    Q_PROPERTY(QVariantList members READ members NOTIFY stateChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendChanged)
    Q_PROPERTY(bool joining READ joining NOTIFY sendChanged)
    Q_PROPERTY(QString actionErrorKey READ actionErrorKey NOTIFY sendChanged)
    Q_PROPERTY(ChatModel *chat READ chat CONSTANT)
public:
    RoomController(ApiClient *api, Session *session, Preferences *preferences, QObject *parent = nullptr);

    QString roomId() const { return m_roomId; }
    bool loading() const { return m_loading; }
    QString errorKey() const { return m_errorKey; }
    QString name() const { return m_room.name; }
    QString description() const { return m_room.description; }
    QString format() const { return m_room.format; }
    QString visibility() const { return m_room.visibility; }
    QString source() const { return m_room.url; }
    QString fandomId() const { return m_room.fandomId; }
    bool joined() const { return m_joined; }
    bool closed() const { return m_room.closed; }
    QString role() const { return m_role; }
    QVariantList members() const { return m_members; }
    bool sending() const { return m_sending; }
    bool joining() const { return m_joining; }
    QString actionErrorKey() const { return m_actionErrorKey; }
    ChatModel *chat() { return &m_chat; }

    Q_INVOKABLE void open(const QString &roomId);
    Q_INVOKABLE void close();
    Q_INVOKABLE void join();
    /** Starts sending; returns false (and keeps the text) when invalid. */
    Q_INVOKABLE bool send(const QString &text);
    Q_INVOKABLE void retry() { load(true); }

    /** Applies a snapshot body (public for tests). */
    void applySnapshot(const QJsonObject &body);

signals:
    void roomChanged();
    void stateChanged();
    void sendChanged();
    /** A message was accepted by the server; the composer may clear. */
    void sent();

private:
    void load(bool full);
    void schedule();
    void ping();

    ApiClient *m_api;
    Session *m_session;
    Preferences *m_preferences;
    ChatModel m_chat;
    QTimer m_poll, m_occupancy;
    QString m_roomId, m_errorKey, m_role, m_actionErrorKey;
    QString m_viewer; // the account the current state was loaded for
    RoomRecord m_room;
    QVariantList m_members;
    bool m_loading = false, m_joined = false, m_sending = false, m_joining = false, m_inFlight = false;
    quint64 m_generation = 0;
};

} // namespace katsodi
