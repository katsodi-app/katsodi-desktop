#include "presentation/ThemeController.h"

#include "presentation/Preferences.h"

#include <QColor>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStyleHints>

namespace katsodi {

ThemeController::ThemeController(Preferences *preferences, QObject *parent)
    : QObject(parent), m_preferences(preferences)
{
    QFile file(QStringLiteral(":/katsodi/generated/palettes.json"));
    if (file.open(QIODevice::ReadOnly))
        load(file.readAll());
    m_fontFamily = loadFonts();
    connect(preferences, &Preferences::themeChanged, this, &ThemeController::changed);
    if (auto *hints = QGuiApplication::styleHints())
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this] {
            if (m_preferences->theme() == QLatin1StringView("system"))
                emit changed();
        });
}

QString ThemeController::active() const
{
    const QString theme = m_preferences->theme();
    if (theme != QLatin1StringView("system"))
        return theme;
    const auto *hints = QGuiApplication::styleHints();
    return hints && hints->colorScheme() == Qt::ColorScheme::Light ? QStringLiteral("paper")
                                                                   : QStringLiteral("obsidian");
}

bool ThemeController::load(const QByteArray &json)
{
    static const QStringList tokens{
        "background", "surface", "surfaceElevated", "surfaceHover", "foreground",
        "mutedForeground", "border", "borderStrong", "primary", "primaryForeground",
        "accent", "focusRing", "selected", "success", "warning", "danger", "info",
        "overlay", "shadow"};
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    QHash<QString, Palette> loaded;
    for (const QString &name : palettes()) {
        const QJsonObject entry = root.value(name).toObject();
        const QJsonObject colors = entry.value(QLatin1StringView("colors")).toObject();
        Palette palette;
        palette.dark = entry.value(QLatin1StringView("scheme")).toString() != QLatin1StringView("light");
        for (const QString &token : tokens) {
            const QColor color = QColor::fromString(colors.value(token).toString());
            if (!color.isValid())
                return false;
            palette.colors.insert(token, color);
        }
        loaded.insert(name, palette);
    }
    m_palettes = loaded;
    emit changed();
    return true;
}

QString ThemeController::loadFonts()
{
    QString family;
    for (const char *subset : {"latin", "cyrillic", "latin-ext", "cyrillic-ext"}) {
        const int id = QFontDatabase::addApplicationFont(
            QStringLiteral(":/katsodi/fonts/manrope-%1.ttf").arg(QLatin1StringView(subset)));
        if (id >= 0 && family.isEmpty()) {
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty())
                family = families.first();
        }
    }
    return family.isEmpty() ? QStringLiteral("Segoe UI") : family;
}

} // namespace katsodi
