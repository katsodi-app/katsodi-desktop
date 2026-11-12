#pragma once
#include <QHash>
#include <QObject>
#include <QVariantMap>

namespace katsodi {

class Preferences;

/**
 * Interface text from the `native` section of src/lib/i18n.ts, exported to
 * resources/generated/strings.json. QML reads `I18n.s.<id>`, which updates
 * when the language changes.
 */
class Strings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap s READ current NOTIFY changed)
    Q_PROPERTY(QString locale READ locale NOTIFY changed)
public:
    Strings(Preferences *preferences, QObject *parent = nullptr);

    QVariantMap current() const;
    QString locale() const;
    /** Text for an id, or the id itself if missing (visible in tests). */
    Q_INVOKABLE QString text(const QString &id) const;
    /** Text with one {key} placeholder filled in. */
    Q_INVOKABLE QString format(const QString &id, const QString &key, const QString &value) const;

    bool load(const QByteArray &json);

signals:
    void changed();

private:
    Preferences *m_preferences;
    QHash<QString, QVariantMap> m_tables;
};

} // namespace katsodi
