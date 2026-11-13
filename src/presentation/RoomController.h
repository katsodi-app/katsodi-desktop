#pragma once
#include "domain/Records.h"
#include "presentation/ChatModel.h"

#include <QDateTime>
#include <QJsonObject>
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
    Q_PROPERTY(QVariantList queue READ queue NOTIFY stateChanged)
    /** "playing", "paused" or "" (not started / no synchronized timeline). */
    Q_PROPERTY(QString playback READ playback NOTIFY stateChanged)
    /** Seconds into the source at the moment of the last snapshot, estimated. */
    Q_PROPERTY(double playbackPosition READ playbackPosition NOTIFY stateChanged)
    Q_PROPERTY(bool queueing READ queueing NOTIFY sendChanged)
    /** Owner or moderator: may play from the queue and steer the shared timeline. */
    Q_PROPERTY(bool canControl READ canControl NOTIFY stateChanged)
    /** "youtube", "vimeo", "soundcloud" or "" for the current source. */
    Q_PROPERTY(QString syncKind READ syncKind NOTIFY stateChanged)
    /** Server time of the last timeline change, ms since epoch (0 = none). */
    Q_PROPERTY(qint64 playbackStamp READ playbackStamp NOTIFY stateChanged)
    Q_PROPERTY(bool steering READ steering NOTIFY sendChanged)
    /** The room's current link demo: "pending", "live", "paused" or "". */
    Q_PROPERTY(QString demoStatus READ demoStatus NOTIFY stateChanged)
    Q_PROPERTY(QString demoUrl READ demoUrl NOTIFY stateChanged)
    Q_PROPERTY(QString demoUserName READ demoUserName NOTIFY stateChanged)
    Q_PROPERTY(QString demoPausedByName READ demoPausedByName NOTIFY stateChanged)
    /** The demo is the viewer's own (request or presentation). */
    Q_PROPERTY(bool demoMine READ demoMine NOTIFY stateChanged)
    /** The presenter's account id (their identity in the demo room). */
    Q_PROPERTY(QString demoUserId READ demoUserId NOTIFY stateChanged)
    /** A host opened the demo channel to every member (drawing / control). */
    Q_PROPERTY(bool demoChannelOpen READ demoChannelOpen NOTIFY stateChanged)
    Q_PROPERTY(QString viewerId READ viewerId NOTIFY roomChanged)
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
    QVariantList queue() const { return m_queue; }
    QString playback() const { return m_playback; }
    double playbackPosition() const { return m_playbackPosition; }
    bool queueing() const { return m_queueing; }
    bool canControl() const;
    QString syncKind() const;
    qint64 playbackStamp() const { return m_playbackStamp; }
    bool steering() const { return m_steering; }
    QString demoStatus() const { return m_demoStatus; }
    QString demoUrl() const { return m_demoUrl; }
    QString demoUserName() const { return m_demoUserName; }
    QString demoPausedByName() const { return m_demoPausedByName; }
    QString demoUserId() const { return m_demoUser; }
    bool demoChannelOpen() const { return m_demoChannelOpen; }
    /** The signed-in account this room state belongs to (lower case). */
    QString viewerId() const { return m_viewer.toLower(); }
    bool demoMine() const { return !m_demoStatus.isEmpty() && !m_viewer.isEmpty() && m_demoUser == m_viewer.toLower(); }
    /** Applies a demo record against the server's time (public for tests). */
    static QString currentDemoStatus(const QJsonObject &demo, const QDateTime &serverTime);
    /** Removes a queued link (its author or a host; the server decides). */
    Q_INVOKABLE void removeFromQueue(const QString &itemId);
    /** Host: makes a queued link the room's source now. */
    Q_INVOKABLE void playFromQueue(const QString &itemId);
    /**
     * Host: publishes play/pause at a position (seconds) for the current,
     * synchronizable source. False when not allowed locally.
     */
    Q_INVOKABLE bool setPlayback(const QString &state, double position);
    /** Adds a link to the queue; false when the link is not acceptable. */
    Q_INVOKABLE bool addToQueue(const QString &url);

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
    void queued();

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
    QVariantList m_members, m_queue;
    QString m_playback;
    double m_playbackPosition = 0;
    bool m_queueing = false, m_steering = false;
    qint64 m_playbackStamp = 0;
    QString m_demoStatus, m_demoUrl, m_demoUser, m_demoUserName, m_demoPausedByName;
    bool m_demoChannelOpen = false;
    void queueAction(const QString &path, bool remove);
    bool m_loading = false, m_joined = false, m_sending = false, m_joining = false, m_inFlight = false;
    quint64 m_generation = 0;
};

} // namespace katsodi
