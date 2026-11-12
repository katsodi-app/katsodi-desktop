#pragma once
#include <QHash>
#include <QObject>
#include <QVariantMap>

namespace katsodi {

class Preferences;

/**
 * Katsodi's palettes (exported from src/app/signal.css) and the shared
 * metrics of the design system, for QML. `c` holds the colours of the
 * active palette; "system" follows the OS light/dark setting live.
 */
class ThemeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap c READ colors NOTIFY changed)
    Q_PROPERTY(QString active READ active NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(QStringList palettes READ palettes CONSTANT)
public:
    ThemeController(Preferences *preferences, QObject *parent = nullptr);

    QVariantMap colors() const { return m_palettes.value(active()).colors; }
    QString active() const;
    bool dark() const { return m_palettes.value(active()).dark; }
    QString fontFamily() const { return m_fontFamily; }
    QStringList palettes() const { return {"obsidian", "ember", "aurora", "violet", "paper"}; }
    Q_INVOKABLE QVariantMap paletteColors(const QString &name) const { return m_palettes.value(name).colors; }

    /** Loads palettes from JSON; false when a palette or token is missing. */
    bool load(const QByteArray &json);
    /** Registers the bundled Manrope subsets; returns the family name. */
    static QString loadFonts();

signals:
    void changed();

private:
    struct Palette {
        bool dark = true;
        QVariantMap colors;
    };
    Preferences *m_preferences;
    QHash<QString, Palette> m_palettes;
    QString m_fontFamily;
};

} // namespace katsodi
