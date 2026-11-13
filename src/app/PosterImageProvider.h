#pragma once
#include <QQuickAsyncImageProvider>
#include <QThreadPool>

namespace katsodi {

/**
 * "image://poster/<w>x<h>x<r>/<folder>/<file>.webp": a bundled artwork file
 * decoded directly at the requested pixel size (never at full resolution),
 * centre-cropped to fill it and given transparent rounded corners, on a
 * small worker pool. Only files in the application's artwork/fandoms
 * folder can be named; anything else yields nothing.
 */
class PosterImageProvider final : public QQuickAsyncImageProvider {
public:
    explicit PosterImageProvider(QString artworkRoot);
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    /** Parses and validates an id; returns false for anything unexpected. */
    static bool parse(const QString &id, QSize *size, int *radius, QString *relative);
    static QImage render(const QString &file, QSize size, int radius);

private:
    QString m_root;
    QThreadPool m_pool;
};

} // namespace katsodi
