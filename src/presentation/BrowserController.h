#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRect>
#include <QUrl>
#include <QWindow>

namespace katsodi {

/**
 * The shared browser on the presenter's computer: katsodi-player in browser
 * mode, a child window over the stage. It streams the page as JPEG frames
 * (decoded here and handed to the demo's screen-share track) and takes the
 * host's navigation plus other members' input, which ScreenController has
 * already checked against the control list.
 */
class BrowserController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString url READ url NOTIFY navigated)
    Q_PROPERTY(QString title READ title NOTIFY navigated)
    Q_PROPERTY(bool canBack READ canBack NOTIFY navigated)
    Q_PROPERTY(bool canForward READ canForward NOTIFY navigated)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    /** True while a popup covers the stage: the window steps aside. */
    Q_PROPERTY(bool obscured READ obscured WRITE setObscured NOTIFY changed)
public:
    BrowserController(QUrl origin, QString helperPath, QObject *parent = nullptr);
    ~BrowserController() override;

    bool available() const { return !m_helperPath.isEmpty(); }
    bool running() const { return m_helper != nullptr; }
    /** The helper's process id (its browser processes are its children). */
    quint32 processId() const { return m_helper ? quint32(m_helper->processId()) : 0; }
    QString url() const { return m_url; }
    QString title() const { return m_title; }
    bool canBack() const { return m_canBack; }
    bool canForward() const { return m_canForward; }
    QString errorKey() const { return m_errorKey; }
    bool obscured() const { return m_obscured; }
    void setObscured(bool value);

    /** Starts the helper at a site; frames follow at `fps`, at most `box` big. */
    bool start(QWindow *window, const QString &url, QSize box, int fps);
    void stop();
    void input(const QJsonObject &event);

    Q_INVOKABLE void navigate(const QString &url);
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void reload();
    /** Where the stage wants the browser, in the window's logical pixels. */
    Q_INVOKABLE void setSurface(QWindow *window, qreal x, qreal y, qreal width, qreal height, bool visible);

signals:
    void changed();
    void navigated();
    void frame(const QImage &image);
    /** The helper ended without being asked (crash or missing runtime). */
    void failed();

private:
    void send(const QJsonObject &command);
    void readEvents();
    void pushGeometry();

    QUrl m_origin;
    QString m_helperPath;
    QProcess *m_helper = nullptr;
    QByteArray m_buffer;
    QList<QByteArray> m_pending; // commands written before the process ran
    QPointer<QWindow> m_window;
    QRect m_surface, m_sentGeometry;
    bool m_surfaceVisible = false, m_obscured = false, m_sentVisible = false;
    QString m_url, m_title, m_errorKey;
    bool m_canBack = false, m_canForward = false;
    QElapsedTimer m_sinceFrame;
};

} // namespace katsodi
