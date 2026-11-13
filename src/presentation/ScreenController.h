#pragma once

#include "media/ScreenEngine.h"
#include "platform/ProcessAudio.h"
#include "platform/ShareSources.h"

#include <QCapturableWindow>
#include <QElapsedTimer>
#include <QMediaCaptureSession>
#include <QObject>
#include <QPointer>
#include <QScreenCapture>
#include <QTimer>
#include <QHash>
#include <QStringList>
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
    /** The demo is the shared browser (not a captured window or screen). */
    Q_PROPERTY(bool browserSession READ browserSession NOTIFY changed)
    /** Presenter: who may control the shared browser. */
    Q_PROPERTY(QStringList controllers READ controllers NOTIFY controlChanged)
    Q_PROPERTY(bool controlAll READ controlAll NOTIFY controlChanged)
    /** Viewer: the presenter lets this viewer control the shared browser. */
    Q_PROPERTY(bool canControl READ canControl NOTIFY controlChanged)
    /** Presenter: share the sound too (on by default). */
    Q_PROPERTY(bool shareAudio READ shareAudio WRITE setShareAudio NOTIFY audioChanged)
    /** Presenter: "off", "window" (one app), "system" (all but Katsodi) or "unavailable". */
    Q_PROPERTY(QString audioState READ audioState NOTIFY audioChanged)
    /** Viewer: the presenter's sound is being played. */
    Q_PROPERTY(bool remoteAudio READ remoteAudio NOTIFY audioChanged)
    Q_PROPERTY(bool remoteAudioMuted READ remoteAudioMuted WRITE setRemoteAudioMuted NOTIFY audioChanged)
public:
    ScreenController(ApiClient *api, RoomController *room, Preferences *preferences, ScreenEngine *engine,
                     class BrowserController *browser, QObject *parent = nullptr);
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
    bool browserSession() const { return m_browserSession; }
    QStringList controllers() const { return m_controllers; }
    bool controlAll() const { return m_controlAll; }
    bool canControl() const { return m_canControl; }
    bool shareAudio() const { return m_shareAudio; }
    void setShareAudio(bool value);
    QString audioState() const { return m_audioState; }
    bool remoteAudio() const { return m_remoteAudio; }
    bool remoteAudioMuted() const { return m_remoteAudioMuted; }
    void setRemoteAudioMuted(bool value);
    /**
     * Host: open the shared browser at a site. Requests the demo (a host's
     * own request starts at once), then runs the browser on this computer
     * and shares it instead of a captured window.
     */
    Q_INVOKABLE bool startBrowser(const QString &url);
    /** Presenter: let one member (account id) control the browser, or not. */
    Q_INVOKABLE void setControl(const QString &identity, bool allowed);
    /** Presenter: let every member control the browser, or nobody. */
    Q_INVOKABLE void setControlAll(bool allowed);
    /** Viewer with control: a click, wheel or key on the shared page. */
    Q_INVOKABLE void sendInput(const QVariantMap &event);
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
    void controlChanged();
    void audioChanged();

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
    void pushImage(const QImage &image);
    void beginBrowser();
    void onData(const QString &identity, const QByteArray &payload);
    void announceControl();
    void syncChannel();
    void startSound();
    void stopSound();

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
    // Shared browser.
    class BrowserController *m_browser = nullptr;
    QString m_pendingBrowser; // site asked for, waiting for the demo to start
    bool m_browserSession = false;
    QStringList m_controllers;
    bool m_controlAll = false, m_canControl = false, m_lastChannelOpen = false;
    QTimer m_announce;
    QHash<QString, QPair<qint64, int>> m_rate; // identity -> (second, messages)
    QElapsedTimer m_clock;
    // Shared sound.
    ProcessAudio m_audio;
    bool m_shareAudio = true, m_remoteAudio = false, m_remoteAudioMuted = false;
    QString m_audioState = QStringLiteral("off");
    quint32 m_shareProcess = 0; // the shared window's process (0: a screen)
};

} // namespace katsodi
