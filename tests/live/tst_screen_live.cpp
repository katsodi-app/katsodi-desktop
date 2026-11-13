// Live check of screen sharing against the LiveKit project in .env.local:
// run tools/screen-live-check.mjs, which mints a presenter token (may
// publish a screen share) and a viewer token (may only subscribe) for a
// throwaway room and starts this test.
//
// It captures only its own test window (Windows Graphics Capture through
// Qt Multimedia), never the screen or another application.

#include "media/livekit/LiveKitScreenEngine.h"
#include "platform/ShareSources.h"
#include "presentation/ScreenController.h"

#include <QCapturableWindow>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMediaCaptureSession>
#include <QPainter>
#include <QRasterWindow>
#include <QSignalSpy>
#include <QTest>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWindow>
#include <QWindowCapture>

#include <algorithm>
#include <memory>

using namespace katsodi;

// A window that paints a moving pattern, so captured frames change.
class Pattern : public QRasterWindow {
public:
    int tick = 0;
    Pattern() { setFlag(Qt::WindowDoesNotAcceptFocus); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(), size()), QColor(30, 160, 90)); // known green
        painter.fillRect(QRect((tick * 13) % qMax(1, width() - 80), 20, 80, 80), Qt::white);
    }
};

class ScreenLive : public QObject {
    Q_OBJECT
    QString m_url, m_presenter, m_viewer;

    static bool waitFor(const std::function<bool()> &condition, int ms)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            if (condition())
                return true;
            QTest::qWait(10);
        }
        return false;
    }

private slots:
    void initTestCase()
    {
        m_url = qEnvironmentVariable("KATSODI_LK_URL");
        m_presenter = qEnvironmentVariable("KATSODI_LK_TOKEN_A");
        m_viewer = qEnvironmentVariable("KATSODI_LK_TOKEN_B");
    }

    // The picker's list (Win32) finds a real window with a preview and icon,
    // and that window can be captured by its handle, as the client does.
    // Only this test's own window is captured.
    void pickerFindsAndCapturesAWindowByHandle()
    {
        Pattern window;
        window.setTitle(QStringLiteral("katsodi share check"));
        window.resize(640, 360);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(200);
        QElapsedTimer timer;
        timer.start();
        const QList<ShareSource> withSelf = listWindowSources(0);
        const qint64 listed = timer.elapsed();
        const auto found = std::find_if(withSelf.begin(), withSelf.end(), [&](const ShareSource &s) {
            return s.window == quintptr(window.winId());
        });
        QVERIFY2(found != withSelf.end(), "the test window was not listed");
        QCOMPARE(found->title, QStringLiteral("katsodi share check"));
        QVERIFY(!found->preview.isNull());
        // The pattern's green shows in the preview.
        const QColor colour = found->preview.pixelColor(found->preview.width() / 2, found->preview.height() * 3 / 4);
        QVERIFY2(qAbs(colour.green() - 160) < 30, qPrintable(colour.name()));
        qInfo("listed %lld windows with previews in %lld ms", qint64(withSelf.size()), listed);
        // The client excludes its own windows.
        for (const ShareSource &s : listWindowSources(quint32(QCoreApplication::applicationPid())))
            QVERIFY(s.window != quintptr(window.winId()));

        std::unique_ptr<QWindow> foreign(QWindow::fromWinId(window.winId()));
        const QCapturableWindow capturable(foreign.get());
        QVERIFY(capturable.isValid());
        QMediaCaptureSession capture;
        QWindowCapture windowCapture;
        QVideoSink sink;
        capture.setWindowCapture(&windowCapture);
        capture.setVideoSink(&sink);
        windowCapture.setWindow(capturable);
        int frames = 0;
        connect(&sink, &QVideoSink::videoFrameChanged, this, [&](const QVideoFrame &frame) { frames += frame.isValid(); });
        windowCapture.start();
        QVERIFY2(waitFor([&] { return frames > 0; }, 10000), "no frame from the window captured by handle");
        windowCapture.stop();
    }

    void publishedSizeFitsTheBox()
    {
        QCOMPARE(ScreenController::publishedSize({2560, 1440}, 1080), QSize(1920, 1080));
        QCOMPARE(ScreenController::publishedSize({2560, 1440}, 720), QSize(1280, 720));
        QCOMPARE(ScreenController::publishedSize({801, 603}, 1080), QSize(800, 600));
        QCOMPARE(ScreenController::publishedSize({1444, 822}, 1080), QSize(1444, 820));
    }

    void presenterToViewer()
    {
        if (m_url.isEmpty() || m_presenter.isEmpty() || m_viewer.isEmpty())
            QSKIP("run tools/screen-live-check.mjs");
        LiveKitScreenEngine presenter, viewer;
        QVideoSink sink;
        viewer.setVideoSink(&sink);
        QSignalSpy presenterUp(&presenter, &ScreenEngine::connected);
        QSignalSpy viewerUp(&viewer, &ScreenEngine::connected);
        QSignalSpy remote(&viewer, &ScreenEngine::remoteScreenChanged);
        QSignalSpy publishing(&presenter, &ScreenEngine::publishingChanged);
        QElapsedTimer timer;
        timer.start();
        presenter.connectRoom(1, m_url, m_presenter);
        viewer.connectRoom(1, m_url, m_viewer);
        QVERIFY(waitFor([&] { return presenterUp.count() && viewerUp.count(); }, 20000));
        qInfo("both connected after %lld ms", timer.elapsed());

        // Capture this test's own window, as the client captures a window.
        Pattern window;
        window.resize(960, 540);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QMediaCaptureSession capture;
        QWindowCapture windowCapture;
        QVideoSink captured;
        capture.setWindowCapture(&windowCapture);
        capture.setVideoSink(&captured);
        windowCapture.setWindow(QCapturableWindow(&window));
        int capturedFrames = 0;
        QSize capturedSize;
        bool started = false;
        QElapsedTimer sincePush;
        connect(&captured, &QVideoSink::videoFrameChanged, this, [&](const QVideoFrame &frame) {
            ++capturedFrames;
            if (sincePush.isValid() && sincePush.elapsed() < 66)
                return;
            sincePush.start();
            const QImage image = frame.toImage();
            if (image.isNull())
                return;
            capturedSize = image.size();
            if (!started) {
                started = true;
                presenter.startPublishing(ScreenController::publishedSize(image.size(), 1080), 15, 2'500'000);
            }
            presenter.pushFrame(image);
        });
        windowCapture.start();
        QTimer animate;
        connect(&animate, &QTimer::timeout, &window, [&] { ++window.tick; window.requestUpdate(); window.update(); });
        animate.start(50);

        int received = 0;
        QSize receivedSize;
        QColor centre;
        connect(&sink, &QVideoSink::videoFrameChanged, this, [&](const QVideoFrame &frame) {
            ++received;
            receivedSize = frame.size();
            if (received % 10 == 1) {
                const QImage image = frame.toImage();
                if (!image.isNull())
                    centre = image.pixelColor(image.width() / 2, image.height() * 3 / 4);
            }
        });
        timer.restart();
        QVERIFY2(waitFor([&] { return capturedFrames > 0; }, 10000), "no frame from window capture");
        qInfo("window capture: first frame after %lld ms, %dx%d", timer.elapsed(), capturedSize.width(),
              capturedSize.height());
        QVERIFY(waitFor([&] { return publishing.count() > 0; }, 10000));
        QVERIFY2(waitFor([&] { return received > 0; }, 15000), "viewer received no frame");
        qInfo("viewer: first frame %lld ms after capture started, %dx%d", timer.elapsed(), receivedSize.width(),
              receivedSize.height());
        QVERIFY(!remote.isEmpty() && remote.first().at(0).toBool());
        QCOMPARE(receivedSize, ScreenController::publishedSize(capturedSize, 1080));
        QVERIFY(waitFor([&] { return received >= 30; }, 15000));
        // The pattern's green survives encoding (tolerance for compression).
        qInfo("centre colour %d,%d,%d after %d frames", centre.red(), centre.green(), centre.blue(), received);
        QVERIFY(qAbs(centre.red() - 30) < 40 && qAbs(centre.green() - 160) < 40 && qAbs(centre.blue() - 90) < 40);

        // A host's pause: the track stays, frames stop.
        presenter.setPaused(true);
        QTest::qWait(1500);
        const int beforePause = received;
        QTest::qWait(2500);
        qInfo("frames while paused: %d", received - beforePause);
        QVERIFY(received - beforePause <= 2);
        presenter.setPaused(false);
        const int afterResume = received;
        QVERIFY2(waitFor([&] { return received > afterResume + 5; }, 10000), "frames did not resume");

        // Stopping the share ends the viewer's screen.
        windowCapture.stop();
        presenter.stopPublishing();
        QVERIFY(waitFor([&] { return remote.count() >= 2 && !remote.last().at(0).toBool(); }, 15000));
        qInfo("viewer saw the screen end; %d frames received in total", received);
        presenter.disconnectRoom();
        viewer.disconnectRoom();
        QTest::qWait(500);
    }
};

QTEST_MAIN(ScreenLive)
#include "tst_screen_live.moc"
