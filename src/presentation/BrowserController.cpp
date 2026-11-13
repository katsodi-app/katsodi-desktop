#include "presentation/BrowserController.h"

#include <QJsonDocument>
#include <QTimer>

namespace katsodi {
namespace {
// Frames are base64 JPEG lines; a 1080p frame stays well below this.
constexpr qsizetype maxLine = 6 * 1024 * 1024;
} // namespace

BrowserController::BrowserController(QUrl origin, QString helperPath, QObject *parent)
    : QObject(parent), m_origin(std::move(origin)), m_helperPath(std::move(helperPath))
{
}

BrowserController::~BrowserController()
{
    if (!m_helper)
        return;
    m_helper->setProperty("stopping", true);
    m_helper->closeWriteChannel();
    if (!m_helper->waitForFinished(2000))
        m_helper->kill();
    m_helper->waitForFinished(1000);
}

bool BrowserController::start(QWindow *window, const QString &url, QSize box, int fps)
{
    if (m_helper || !available() || !window)
        return false;
    m_window = window;
    m_errorKey.clear();
    m_helper = new QProcess(this);
    m_helper->setProgram(m_helperPath);
    m_helper->setArguments({QStringLiteral("--mode"), QStringLiteral("browser"), QStringLiteral("--origin"),
                            m_origin.toString(QUrl::RemovePath | QUrl::StripTrailingSlash), QStringLiteral("--parent"),
                            QString::number(quintptr(window->winId()))});
    m_helper->setProcessChannelMode(QProcess::SeparateChannels);
    m_helper->setStandardErrorFile(QProcess::nullDevice());
    connect(m_helper, &QProcess::readyReadStandardOutput, this, &BrowserController::readEvents);
    // Commands sent before the process runs are kept and written once it does.
    connect(m_helper, &QProcess::started, this, [this] {
        for (const QByteArray &line : std::exchange(m_pending, {}))
            m_helper->write(line);
    });
    connect(m_helper, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        const bool expected = m_helper->property("stopping").toBool();
        m_helper->deleteLater();
        m_helper = nullptr;
        m_buffer.clear();
        m_pending.clear();
        m_sentGeometry = {};
        m_sentVisible = false;
        if (!expected) {
            m_errorKey = QStringLiteral("browserFailed");
            emit failed();
        }
        emit changed();
    });
    m_helper->start();
    send({{"op", "screencast"}, {"fps", fps}, {"maxWidth", box.width()}, {"maxHeight", box.height()}});
    send({{"op", "navigate"}, {"url", url}});
    pushGeometry();
    emit changed();
    return true;
}

void BrowserController::stop()
{
    if (!m_helper)
        return;
    m_helper->setProperty("stopping", true);
    m_helper->closeWriteChannel();
    QPointer<QProcess> helper = m_helper;
    QTimer::singleShot(3000, this, [helper] {
        if (helper && helper->state() != QProcess::NotRunning)
            helper->kill();
    });
    m_url.clear();
    m_title.clear();
    m_canBack = m_canForward = false;
    emit navigated();
}

void BrowserController::input(const QJsonObject &event) { send({{"op", "input"}, {"type", event.value("type")},
                                                                {"action", event.value("action")}, {"x", event.value("x")},
                                                                {"y", event.value("y")}, {"button", event.value("button")},
                                                                {"clicks", event.value("clicks")}, {"dx", event.value("dx")},
                                                                {"dy", event.value("dy")}, {"key", event.value("key")},
                                                                {"code", event.value("code")}, {"vk", event.value("vk")},
                                                                {"modifiers", event.value("modifiers")},
                                                                {"text", event.value("text")}}); }

void BrowserController::navigate(const QString &url)
{
    if (url.size() <= 2048)
        send({{"op", "navigate"}, {"url", url}});
}
void BrowserController::back() { send({{"op", "back"}}); }
void BrowserController::forward() { send({{"op", "forward"}}); }
void BrowserController::reload() { send({{"op", "reload"}}); }

void BrowserController::setObscured(bool value)
{
    if (value == m_obscured)
        return;
    m_obscured = value;
    emit changed();
    pushGeometry();
}

void BrowserController::setSurface(QWindow *window, qreal x, qreal y, qreal width, qreal height, bool visible)
{
    const QRect rect(qRound(x), qRound(y), qRound(width), qRound(height));
    if (window == m_window && rect == m_surface && visible == m_surfaceVisible)
        return;
    m_surface = rect;
    m_surfaceVisible = visible && rect.width() > 0 && rect.height() > 0;
    pushGeometry();
}

void BrowserController::pushGeometry()
{
    if (!m_helper)
        return;
    const bool show = m_surfaceVisible && !m_obscured;
    if (show && m_surface != m_sentGeometry) {
        m_sentGeometry = m_surface;
        send({{"op", "geometry"}, {"x", m_surface.x()}, {"y", m_surface.y()}, {"w", m_surface.width()},
              {"h", m_surface.height()}});
    }
    if (show != m_sentVisible) {
        m_sentVisible = show;
        // Hidden only from the host: the page keeps painting for the others.
        send({{"op", "visible"}, {"value", show}});
    }
}

void BrowserController::send(const QJsonObject &command)
{
    if (!m_helper)
        return;
    const QByteArray line = QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n';
    if (m_helper->state() == QProcess::Running)
        m_helper->write(line);
    else if (m_helper->state() == QProcess::Starting && m_pending.size() < 64)
        m_pending.append(line);
}

void BrowserController::readEvents()
{
    m_buffer += m_helper->readAllStandardOutput();
    qsizetype end;
    while ((end = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(end);
        m_buffer.remove(0, end + 1);
        if (line.size() > maxLine)
            continue;
        const QJsonObject event = QJsonDocument::fromJson(line).object();
        const QString kind = event.value(QLatin1StringView("ev")).toString();
        if (kind == QLatin1StringView("frame")) {
            const QImage image = QImage::fromData(
                QByteArray::fromBase64(event.value(QLatin1StringView("jpeg")).toString().toLatin1()), "JPEG");
            if (!image.isNull())
                emit frame(image);
        } else if (kind == QLatin1StringView("nav")) {
            m_url = event.value(QLatin1StringView("url")).toString().left(2048);
            m_title = event.value(QLatin1StringView("title")).toString().left(200);
            m_canBack = event.value(QLatin1StringView("canBack")).toBool();
            m_canForward = event.value(QLatin1StringView("canForward")).toBool();
            emit navigated();
        } else if (kind == QLatin1StringView("error")) {
            m_errorKey = QStringLiteral("browserFailed");
            emit changed();
        }
    }
    if (m_buffer.size() > maxLine)
        m_buffer.clear();
}

} // namespace katsodi
