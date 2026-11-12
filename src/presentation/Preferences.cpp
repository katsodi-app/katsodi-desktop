#include "presentation/Preferences.h"

#include <QLocale>
#include <QThread>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace katsodi {
namespace {

quint64 physicalMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return status.ullTotalPhys;
#endif
    return 0; // unknown: never counts as "weak"
}

bool systemAnimationsOff()
{
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
        return enabled == FALSE;
#endif
    return false;
}

} // namespace

Preferences::Preferences(const QString &application, QObject *parent)
    : QObject(parent), m_settings(QSettings::defaultFormat(), QSettings::UserScope, QStringLiteral("Katsodi"), application)
{
    const QString theme = m_settings.value(QStringLiteral("theme")).toString();
    m_theme = isTheme(theme) ? theme : QStringLiteral("obsidian");
    const QString locale = m_settings.value(QStringLiteral("locale")).toString();
    m_locale = locale == QLatin1StringView("ru") || locale == QLatin1StringView("en")
                   ? locale
                   : (QLocale::system().language() == QLocale::Russian ? QStringLiteral("ru")
                                                                       : QStringLiteral("en"));
    const QString mode = m_settings.value(QStringLiteral("resourceMode")).toString();
    m_resourceMode = mode == QLatin1StringView("standard") || mode == QLatin1StringView("low")
                         ? mode
                         : QStringLiteral("auto");
}

bool Preferences::isTheme(const QString &value)
{
    static const QStringList themes{QStringLiteral("obsidian"), QStringLiteral("ember"),
                                    QStringLiteral("aurora"),   QStringLiteral("violet"),
                                    QStringLiteral("paper"),    QStringLiteral("system")};
    return themes.contains(value);
}

bool Preferences::autoLow(quint64 memoryBytes, int threads)
{
    constexpr quint64 twoGiB = 2ull * 1024 * 1024 * 1024;
    return (memoryBytes > 0 && memoryBytes <= twoGiB) || (threads > 0 && threads <= 2);
}

bool Preferences::lowResources() const
{
    if (m_resourceMode == QLatin1StringView("low"))
        return true;
    if (m_resourceMode == QLatin1StringView("standard"))
        return false;
    return autoLow(physicalMemory(), QThread::idealThreadCount());
}

bool Preferences::reduceMotion() const { return lowResources() || systemAnimationsOff(); }

void Preferences::setTheme(const QString &theme)
{
    if (!isTheme(theme) || theme == m_theme)
        return;
    m_theme = theme;
    m_settings.setValue(QStringLiteral("theme"), theme);
    emit themeChanged();
}

void Preferences::setLocale(const QString &locale)
{
    if ((locale != QLatin1StringView("ru") && locale != QLatin1StringView("en")) || locale == m_locale)
        return;
    m_locale = locale;
    m_settings.setValue(QStringLiteral("locale"), locale);
    emit localeChanged();
}

void Preferences::setResourceMode(const QString &mode)
{
    if ((mode != QLatin1StringView("auto") && mode != QLatin1StringView("standard")
         && mode != QLatin1StringView("low"))
        || mode == m_resourceMode)
        return;
    m_resourceMode = mode;
    m_settings.setValue(QStringLiteral("resourceMode"), mode);
    emit resourceModeChanged();
}

QRect Preferences::windowGeometry() const
{
    const QRect rect = m_settings.value(QStringLiteral("window/geometry")).toRect();
    return rect.width() >= 320 && rect.height() >= 420 && rect.width() <= 16384 && rect.height() <= 16384
               ? rect
               : QRect();
}

bool Preferences::windowMaximized() const
{
    return m_settings.value(QStringLiteral("window/maximized")).toBool();
}

void Preferences::saveWindow(const QRect &normal, bool maximized)
{
    m_settings.setValue(QStringLiteral("window/geometry"), normal);
    m_settings.setValue(QStringLiteral("window/maximized"), maximized);
}

} // namespace katsodi
