#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QWindow>

namespace katsodi {

class RoomController;
class Preferences;

/**
 * The room's official provider player (YouTube, Vimeo, SoundCloud), shown
 * by the separate katsodi-player helper as a child window over the stage.
 *
 * - The helper starts only while an open room has a synchronizable source
 *   and a visible stage has asked for it; it stops 20 s after the room no
 *   longer needs it, and with the client.
 * - Commands and events travel over the helper's own stdin/stdout pipes.
 * - Shared timeline as on the website: followers apply the server's
 *   play/pause/position (re-seeking past 1.5 s of drift); a host's own
 *   play/pause/seek is published, never the echo of applied server state.
 *   A host's finished link plays the next queued one.
 */
class PlayerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    /** "off", "starting", "loading", "ready" or "error". */
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY progress)
    Q_PROPERTY(double position READ position NOTIFY progress)
    Q_PROPERTY(double duration READ duration NOTIFY progress)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    /** True while a popup covers the stage: the helper window hides. */
    Q_PROPERTY(bool obscured READ obscured WRITE setObscured NOTIFY changed)
public:
    PlayerController(RoomController *room, Preferences *preferences, QUrl origin, QString helperPath,
                     QObject *parent = nullptr);
    ~PlayerController() override;

    bool available() const { return !m_helperPath.isEmpty(); }
    QString state() const { return m_state; }
    QString errorKey() const { return m_errorKey; }
    bool playing() const { return m_playing; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
    void setVolume(int value);
    bool obscured() const { return m_obscured; }
    void setObscured(bool value);

    /**
     * Where the stage wants the player, in the window's logical pixels.
     * Called by the stage on every frame it moves; unchanged values are free.
     */
    Q_INVOKABLE void setSurface(QWindow *window, qreal x, qreal y, qreal width, qreal height, bool visible);
    /** Host: publishes play/pause. Others: local play/pause. */
    Q_INVOKABLE void togglePlay();
    /** Host only: publishes a new position. */
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void retry();
    /** Stops the helper now (client exit). */
    void shutdown();

    /** Where the shared timeline is now, from the last snapshot (public for tests). */
    static double expectedPosition(const QString &state, double position, qint64 elapsedMs);

signals:
    void changed();
    void progress();
    void volumeChanged();
    /** The provider asked to open a page (e.g. "Watch on YouTube"); https only. */
    void externalRequested(const QString &url);

private:
    void roomChanged();
    void ensureHelper();
    void stopHelper();
    void send(const QJsonObject &command);
    void readEvents();
    void handle(const QJsonObject &event);
    void applyServer();
    void publish(const QString &state, double position);
    void setState(const QString &state, const QString &errorKey = {});
    void pushGeometry();
    bool suppressed() const { return m_suppress.isValid() && m_suppress.elapsed() < 2500; }

    RoomController *m_room;
    Preferences *m_preferences;
    QUrl m_origin;
    QString m_helperPath;
    QProcess *m_helper = nullptr;
    QByteArray m_buffer;
    QTimer m_idle;
    QPointer<QWindow> m_window;
    QRect m_surface;
    bool m_surfaceVisible = false, m_obscured = false, m_sentVisible = false;
    QRect m_sentGeometry;
    QString m_state = QStringLiteral("off"), m_errorKey;
    QString m_loadedSource;
    qint64 m_generation = 0, m_appliedStamp = -1;
    bool m_ready = false, m_playing = false, m_wantedByStage = false;
    double m_position = 0, m_duration = 0, m_lastPosition = 0, m_serverPosition = -1;
    int m_volume = 80;
    QElapsedTimer m_receivedAt, m_suppress, m_sinceLastPosition, m_sinceDriftFix;
    QString m_pendingState;
    double m_pendingPosition = -1;
};

} // namespace katsodi
