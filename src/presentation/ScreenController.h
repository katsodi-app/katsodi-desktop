#pragma once

#include "media/ScreenEngine.h"
#include "platform/ShareSources.h"

#include <QCapturableWindow>
#include <QElapsedTimer>
#include <QMediaCaptureSession>
#include <QObject>
#include <QPointer>
#include <QScreenCapture>
#include <QTimer>
#include <QVariantList>
#include <QVideoSink>
#include <QWindow>
#include <QWindowCapture>
#include <memory>

namespace katsodi {

class ApiClient;
class RoomController;
class Preferences;

/**
 * Link demos (screen sharing) in the open room, by the website's rules:
 *
 * - Anyone in the room asks to demo a link; a host's own request starts at
 *   once, a member's waits for an owner or moderator to accept it.
 * - While a demo runs, members connect to the room's separate demo SFU room
 *   with a fresh 60-second token and watch the presenter's screen.
 * - The presenter picks a screen or a window (Windows Graphics Capture via
 *   Qt Multimedia), and its frames are published as a screen-share track,
 *   at most 1920×1080 at 15 fps (1280×720 at 8 fps in Low resource mode).
 *   The presenter confirms the demo every 20 s; a refused confirmation
 *   (ended by a host) stops sharing. A host's pause stops sending frames.
 * - Leaving the room, a demo ending or closing the app ends the capture and
 *   the connection.
 */
class ScreenController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    /** "off", "connecting", "connected" or "error". */
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    /** A presenter's screen is being received. */
    Q_PROPERTY(bool remote READ remote NOTIFY changed)
    Q_PROPERTY(qreal remoteAspect READ remoteAspect NOTIFY changed)
    /** This client is capturing and publishing its screen. */
    Q_PROPERTY(bool sharing READ sharing NOTIFY changed)
    /** The current running demo is this viewer's: it may share. */
    Q_PROPERTY(bool presenter READ presenter NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    /** Screens and windows to share: [{ index, kind, name, preview, icon, minimized }]. */
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(bool loadingSources READ loadingSources NOTIFY sourcesChanged)
    /** Published height box: 720 or 1080. */
    Q_PROPERTY(int quality READ quality WRITE setQuality NOTIFY qualityChanged)
    /** 15 or 30. */
    Q_PROPERTY(int framesPerSecond READ framesPerSecond WRITE setFramesPerSecond NOTIFY qualityChanged)
public:
    ScreenController(ApiClient *api, RoomController *room, Preferences *preferences, ScreenEngine *engine,
                     QObject *parent = nullptr);
    ~ScreenController() override;

    bool available() const { return m_engine->available(); }
    QString state() const { return m_state; }
    QString errorKey() const { return m_errorKey; }
    bool remote() const { return m_remote; }
    qreal remoteAspect() const { return m_remoteAspect; }
    bool sharing() const { return m_sharing; }
    bool presenter() const;
    bool busy() const { return m_busy; }
    QVariantList sources() const { return m_sourceList; }
    bool loadingSources() const { return m_loadingSources; }
    int quality() const { return m_quality; }
    void setQuality(int value);
    int framesPerSecond() const { return m_framesPerSecond; }
    void setFramesPerSecond(int value);
    /** Where the presenter sees their own shared picture (may be null). */
    Q_INVOKABLE void setPreviewSink(QVideoSink *sink);

    /** The room screen is shown: watching is wanted (decoding costs CPU). */
    Q_INVOKABLE void setWanted(bool wanted);
    Q_INVOKABLE void setVideoSink(QVideoSink *sink);
    Q_INVOKABLE void refreshSources();
    /** Presenter: start capturing sources[index] and publishing it. */
    Q_INVOKABLE void share(int index);
    /** Presenter: stop sharing and end the demo, as on the website. */
    Q_INVOKABLE void stopSharing();
    /** Ask to demo a link (posted to the chat as well). False when not acceptable. */
    Q_INVOKABLE bool request(const QString &url);
    /** Host: accept or decline a waiting request. */
    Q_INVOKABLE void decide(bool accept);
    /** Host: pause or resume the running demo. */
    Q_INVOKABLE void togglePause();
    /** End the demo (presenter or host) or withdraw one's own request. */
    Q_INVOKABLE void stop();
    /** Ends capture and connection now (client exit). */
    void shutdown();

    /** Size a captured frame is published at, in a 720p or 1080p box (public for tests). */
    static QSize publishedSize(QSize captured, int boxHeight);
    /** Encoder bitrate for a quality and frame rate (public for tests). */
    static int bitrateFor(int boxHeight, int framesPerSecond);

signals:
    void changed();
    void sourcesChanged();
    void qualityChanged();

private:
    void roomChanged();
    void connectDemo();
    void disconnectDemo();
    void beat();
    void onFrame(const QVideoFrame &frame);
    void stopCapture();
    void post(const QString &suffix, const QJsonObject &body, bool remove = false);
    void setState(const QString &state, const QString &errorKey = {});
    void publishSources();

    ApiClient *m_api;
    RoomController *m_room;
    Preferences *m_preferences;
    ScreenEngine *m_engine;

    QString m_state = QStringLiteral("off"), m_errorKey;
    bool m_wanted = false, m_remote = false, m_sharing = false, m_busy = false, m_publishStarted = false;
    bool m_mayPublish = false;
    qreal m_remoteAspect = 16.0 / 9.0;
    QString m_roomId; // the room the connection belongs to
    quint64 m_generation = 0, m_connected = 0;
    int m_retries = 0;
    QTimer m_beat, m_retry;

    QMediaCaptureSession m_capture;
    QScreenCapture m_screenCapture;
    QWindowCapture m_windowCapture;
    QVideoSink m_captureSink;
    QElapsedTimer m_sinceFrame;
    QVariantList m_sourceList;
    QList<ShareSource> m_sources;
    quint64 m_sourcesGeneration = 0;
    bool m_loadingSources = false;
    std::unique_ptr<QWindow> m_foreign; // the shared window's handle, wrapped
    QPointer<QVideoSink> m_previewSink;
    int m_quality = 1080, m_framesPerSecond = 15;
};

} // namespace katsodi
