#pragma once
#include <QObject>
#include <QRect>
#include <QSettings>

namespace katsodi {

/**
 * Small local preferences (theme, language, resource mode, window
 * geometry), validated on read. Nothing about the account is stored here.
 */
class Preferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QString resourceMode READ resourceMode WRITE setResourceMode NOTIFY resourceModeChanged)
    Q_PROPERTY(bool lowResources READ lowResources NOTIFY resourceModeChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY resourceModeChanged)
public:
    explicit Preferences(const QString &application, QObject *parent = nullptr);

    QString theme() const { return m_theme; }
    QString locale() const { return m_locale; }
    QString resourceMode() const { return m_resourceMode; }
    bool lowResources() const;
    /** Low resources, or the system asked for fewer animations. */
    bool reduceMotion() const;

    void setTheme(const QString &theme);
    void setLocale(const QString &locale);
    void setResourceMode(const QString &mode);

    QRect windowGeometry() const;
    bool windowMaximized() const;
    void saveWindow(const QRect &normal, bool maximized);

    static bool isTheme(const QString &value);
    /** Auto picks Low only on a clear hint: ≤ 2 GiB RAM or ≤ 2 threads. */
    static bool autoLow(quint64 memoryBytes, int threads);

signals:
    void themeChanged();
    void localeChanged();
    void resourceModeChanged();

private:
    QSettings m_settings;
    QString m_theme, m_locale, m_resourceMode;
};

} // namespace katsodi
