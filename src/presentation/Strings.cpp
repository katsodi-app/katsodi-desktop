#include "presentation/Strings.h"

#include "domain/Text.h"
#include "presentation/Preferences.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace katsodi {

Strings::Strings(Preferences *preferences, QObject *parent) : QObject(parent), m_preferences(preferences)
{
    QFile file(QStringLiteral(":/katsodi/generated/strings.json"));
    if (file.open(QIODevice::ReadOnly))
        load(file.readAll());
    connect(preferences, &Preferences::localeChanged, this, &Strings::changed);
}

bool Strings::load(const QByteArray &json)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonObject ru = root.value(QLatin1StringView("ru")).toObject();
    const QJsonObject en = root.value(QLatin1StringView("en")).toObject();
    if (ru.isEmpty() || ru.keys() != en.keys())
        return false;
    m_tables.insert(QStringLiteral("ru"), ru.toVariantMap());
    m_tables.insert(QStringLiteral("en"), en.toVariantMap());
    emit changed();
    return true;
}

QString Strings::locale() const { return m_preferences->locale(); }

QVariantMap Strings::current() const { return m_tables.value(locale()); }

QString Strings::text(const QString &id) const
{
    const QVariant value = m_tables.value(locale()).value(id);
    return value.isValid() ? value.toString() : id;
}

QString Strings::format(const QString &id, const QString &key, const QString &value) const
{
    return formatText(text(id), key, value);
}

} // namespace katsodi
