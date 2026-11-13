// Live check of the provider-player helper against the real providers.
// Needs the standalone API (npm run native:api) for the player page and
// network access to YouTube, Vimeo and SoundCloud. Plays muted.
//
//   tst_player_live [-o report.txt]   (env KATSODI_API_ORIGIN, KATSODI_PLAYER_SHOTS=<folder>)

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QImage>
#include <QQuickWindow>

#include <windows.h>
#include <QScreen>
#include <QTest>

// The client renders with Qt Quick (Direct3D 11 swap chain); the helper's
// child window has to appear over it, so the host is a QQuickWindow too.
class Host : public QQuickWindow {
public:
    Host()
    {
        setColor(QColor(20, 20, 30));
        setFlag(Qt::WindowDoesNotAcceptFocus); // never take focus from what the user is doing
    }
};

class PlayerLive : public QObject {
    Q_OBJECT
    Host m_window;
    QProcess m_helper;
    QByteArray m_buffer;
    QJsonObject m_last;
    QStringList m_events;
    qint64 m_generation = 0;

    void send(const QJsonObject &command)
    {
        m_helper.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
    }
    void pump()
    {
        m_buffer += m_helper.readAllStandardOutput();
        qsizetype end;
        while ((end = m_buffer.indexOf('\n')) >= 0) {
            const QJsonObject event = QJsonDocument::fromJson(m_buffer.left(end)).object();
            m_buffer.remove(0, end + 1);
            if (event.value(QLatin1StringView("ev")).toString() == QLatin1StringView("state")
                && qint64(event.value(QLatin1StringView("generation")).toDouble()) == m_generation) {
                m_last = event;
                for (const QJsonValue &e : event.value(QLatin1StringView("events")).toArray())
                    m_events.append(e.toString());
            }
        }
    }
    bool waitFor(const std::function<bool()> &condition, int ms)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            m_helper.waitForReadyRead(50);
            pump();
            if (condition())
                return true;
        }
        return false;
    }
    QImage ownWindowImage()
    {
        const HWND window = HWND(m_window.winId());
        RECT rect;
        GetClientRect(window, &rect);
        const int w = rect.right, h = rect.bottom;
        HDC screen = GetDC(nullptr);
        HDC memory = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
        HGDIOBJ old = SelectObject(memory, bitmap);
        PrintWindow(window, memory, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
        QImage image(w, h, QImage::Format_ARGB32);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
        GetDIBits(memory, bitmap, 0, UINT(h), image.bits(), &info, DIB_RGB_COLORS);
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return image;
    }
    double position() const { return m_last.value(QLatin1StringView("position")).toDouble(); }
    bool playing() const { return m_last.value(QLatin1StringView("playing")).toBool(); }

    void exercise(const QString &kind, const QString &source)
    {
        ++m_generation;
        m_last = {};
        m_events.clear();
        QElapsedTimer timer;
        timer.start();
        send({{"op", "load"}, {"generation", double(m_generation)}, {"kind", kind}, {"source", source}, {"controls", true}});
        QVERIFY2(waitFor([&] { return m_last.value("ready").toBool() || !m_last.value("error").toString().isEmpty(); }, 40000),
                 "no ready state");
        QVERIFY2(m_last.value("error").toString().isEmpty(), qPrintable(m_last.value("error").toString() + " codecs " + QJsonDocument(m_last.value("codecs").toObject()).toJson(QJsonDocument::Compact)));
        qInfo("%s ready after %lld ms; codecs %s", qPrintable(kind), timer.elapsed(), QJsonDocument(m_last.value("codecs").toObject()).toJson(QJsonDocument::Compact).constData());
        send({{"op", "volume"}, {"value", 0}});
        timer.restart();
        send({{"op", "play"}});
        QVERIFY2(waitFor([&] { return playing() && position() > 1.0; }, 30000), "did not play");
        qInfo("%s playing after %lld ms, position %.1f s, duration %.1f s", qPrintable(kind), timer.elapsed(),
              position(), m_last.value("duration").toDouble());
        if (!qEnvironmentVariableIsEmpty("KATSODI_PLAYER_SHOTS")) {
            // Only this window's own content (with the helper's child window),
            // never the screen: other applications may be on top of it.
            ownWindowImage().save(qEnvironmentVariable("KATSODI_PLAYER_SHOTS") + "/" + kind + ".png");
        }
        send({{"op", "pause"}});
        QVERIFY2(waitFor([&] { return !playing(); }, 10000), "did not pause");
        timer.restart();
        send({{"op", "seek"}, {"position", 30.0}});
        QVERIFY2(waitFor([&] { return std::abs(position() - 30.0) < 2.0; }, 15000), "did not seek");
        qInfo("%s seeked to %.1f s in %lld ms; events %s", qPrintable(kind), position(), timer.elapsed(),
              qPrintable(m_events.join(',')));
        // A command for an older generation must not leak into the next load.
        send({{"op", "unload"}});
    }

private slots:
    void initTestCase()
    {
        m_window.resize(960, 540);
        m_window.setTitle("katsodi-player live check");
        m_window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m_window));
        const QString origin = qEnvironmentVariable("KATSODI_API_ORIGIN", "http://127.0.0.1:8787");
        const QString helper = QCoreApplication::applicationDirPath() + "/katsodi-player.exe";
        m_helper.setProgram(helper);
        m_helper.setArguments({"--origin", origin, "--parent", QString::number(quintptr(m_window.winId()))});
        m_helper.start();
        QVERIFY2(m_helper.waitForStarted(5000), "helper did not start");
        send({{"op", "geometry"}, {"x", 0}, {"y", 0}, {"w", 960}, {"h", 540}});
        send({{"op", "visible"}, {"value", true}});
    }
    void youtube() { exercise("youtube", "https://www.youtube.com/watch?v=aqz-KE-bpKQ"); }
    void vimeo() { exercise("vimeo", "https://vimeo.com/22439234"); }
    void soundcloud() { exercise("soundcloud", "https://soundcloud.com/forss/flickermood"); }
    void refusesUnsupported()
    {
        ++m_generation;
        m_last = {};
        send({{"op", "load"}, {"generation", double(m_generation)}, {"kind", "youtube"},
              {"source", "https://example.com/watch?v=aqz-KE-bpKQ"}, {"controls", true}});
        QVERIFY(waitFor([&] { return m_last.value("error").toString() == "unsupported"; }, 10000));
    }
    void cleanupTestCase()
    {
        QElapsedTimer timer;
        timer.start();
        m_helper.closeWriteChannel();
        const bool finished = m_helper.waitForFinished(15000);
        for (const QByteArray &line : (m_buffer + m_helper.readAllStandardOutput()).split('\n'))
            if (line.contains("closing"))
                qInfo("%s", line.constData());
        QVERIFY2(finished, "helper did not exit when its stdin closed");
        qInfo("helper exited %lld ms after stdin closed", timer.elapsed());
    }
};

QTEST_MAIN(PlayerLive)
#include "tst_player_live.moc"
