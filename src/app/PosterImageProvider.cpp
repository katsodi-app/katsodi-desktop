#include "app/PosterImageProvider.h"

#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QQuickTextureFactory>
#include <QRegularExpression>
#include <QRunnable>

namespace katsodi {
namespace {

class Response final : public QQuickImageResponse, public QRunnable {
public:
    Response(QString file, QSize size, int radius) : m_file(std::move(file)), m_size(size), m_radius(radius)
    {
        setAutoDelete(false);
    }
    void run() override
    {
        m_image = PosterImageProvider::render(m_file, m_size, m_radius);
        emit finished();
    }
    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    QString m_file;
    QSize m_size;
    int m_radius;
    QImage m_image;
};

} // namespace

PosterImageProvider::PosterImageProvider(QString artworkRoot) : m_root(std::move(artworkRoot))
{
    // Two decoders are enough for scrolling; more would only compete.
    m_pool.setMaxThreadCount(2);
}

bool PosterImageProvider::parse(const QString &id, QSize *size, int *radius, QString *relative)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^(\d{1,4})x(\d{1,4})x(\d{1,3})/(fandoms/[a-z0-9-]{1,80}\.webp)$)"));
    const auto match = pattern.match(id);
    if (!match.hasMatch())
        return false;
    const int w = match.captured(1).toInt(), h = match.captured(2).toInt();
    if (w < 1 || h < 1 || w > 4096 || h > 4096)
        return false;
    *size = QSize(w, h);
    *radius = std::min(match.captured(3).toInt(), std::min(w, h) / 2);
    *relative = match.captured(4);
    return true;
}

QImage PosterImageProvider::render(const QString &file, QSize size, int radius)
{
    QImageReader reader(file);
    const QSize original = reader.size();
    if (!original.isValid() || original.isEmpty())
        return {};
    // Decode at the smallest size that still covers the target (crop fill).
    const QSize cover = original.scaled(size, Qt::KeepAspectRatioByExpanding);
    reader.setScaledSize(cover);
    const QImage decoded = reader.read();
    if (decoded.isNull())
        return {};
    const QImage cropped = decoded.copy((cover.width() - size.width()) / 2, (cover.height() - size.height()) / 2,
                                        size.width(), size.height());
    if (radius <= 0)
        return cropped;
    QImage rounded(size, QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(size)), radius, radius);
    painter.setClipPath(path);
    painter.drawImage(0, 0, cropped);
    return rounded;
}

QQuickImageResponse *PosterImageProvider::requestImageResponse(const QString &id, const QSize &)
{
    QSize size;
    int radius = 0;
    QString relative;
    const bool ok = parse(id, &size, &radius, &relative);
    auto *response = new Response(ok ? m_root + u'/' + relative : QString(), size, radius);
    if (ok)
        m_pool.start(response);
    else
        QMetaObject::invokeMethod(response, &QQuickImageResponse::finished, Qt::QueuedConnection);
    return response;
}

} // namespace katsodi
