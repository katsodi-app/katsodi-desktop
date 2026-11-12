#include "domain/Records.h"

#include "domain/Text.h"

#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

namespace katsodi {
namespace {

std::optional<QString> text(const QJsonObject &json, const char *key, qsizetype max,
                            bool allowEmpty = true)
{
    const QJsonValue value = json.value(QLatin1StringView(key));
    if (!value.isString())
        return std::nullopt;
    const QString string = value.toString();
    if (string.size() > max || (!allowEmpty && string.isEmpty()))
        return std::nullopt;
    return string;
}

bool oneOf(const QString &value, std::initializer_list<const char *> allowed)
{
    for (const char *item : allowed)
        if (value == QLatin1StringView(item))
            return true;
    return false;
}

} // namespace

std::optional<QDateTime> parseTimestamp(const QString &value)
{
    if (value.size() > 40)
        return std::nullopt;
    // PostgreSQL sends microseconds (".360928"); Qt parses milliseconds.
    static const QRegularExpression fraction(QStringLiteral("(\\.\\d{3})\\d+"));
    QString normalized = value;
    normalized.replace(fraction, QStringLiteral("\\1"));
    QDateTime parsed = QDateTime::fromString(normalized, Qt::ISODateWithMs);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(normalized, Qt::ISODate);
    if (!parsed.isValid())
        return std::nullopt;
    return parsed.toUTC();
}

std::optional<RoomRecord> parseRoom(const QJsonObject &json)
{
    RoomRecord room;
    const auto id = text(json, "id", 36), owner = text(json, "owner_id", 36),
               name = text(json, "name", 200, false), description = text(json, "description", 2000),
               format = text(json, "format", 10), visibility = text(json, "visibility", 10),
               url = text(json, "url", 4096), art = text(json, "art", 10),
               created = text(json, "created_at", 40);
    if (!id || !owner || !name || !description || !format || !visibility || !url || !art || !created)
        return std::nullopt;
    if (!isUuid(*id) || !isUuid(*owner) || !oneOf(*format, {"video", "music"})
        || !oneOf(*visibility, {"public", "private"}) || !oneOf(*art, {"orbit", "wave", "moon", "sun"}))
        return std::nullopt;
    const auto createdAt = parseTimestamp(*created);
    if (!createdAt)
        return std::nullopt;
    const QJsonValue fandom = json.value(QLatin1StringView("fandom_id"));
    if (!fandom.isNull() && !fandom.isUndefined()) {
        if (!fandom.isString() || fandom.toString().size() > 80)
            return std::nullopt;
        room.fandomId = fandom.toString();
    }
    const QJsonValue closed = json.value(QLatin1StringView("closed_at"));
    room.closed = closed.isString();
    room.id = id->toLower();
    room.ownerId = owner->toLower();
    room.name = *name;
    room.description = *description;
    room.format = *format;
    room.visibility = *visibility;
    room.url = *url;
    room.art = *art;
    room.createdAt = *createdAt;
    return room;
}

std::optional<MessageRecord> parseMessage(const QJsonObject &json)
{
    const auto id = text(json, "id", 36), sender = text(json, "sender_id", 36),
               body = text(json, "body", 4000), created = text(json, "created_at", 40);
    if (!id || !sender || !body || !created || !isUuid(*id) || !isUuid(*sender))
        return std::nullopt;
    const auto createdAt = parseTimestamp(*created);
    if (!createdAt)
        return std::nullopt;
    MessageRecord message;
    message.id = id->toLower();
    message.senderId = sender->toLower();
    message.body = *body;
    message.createdAt = *createdAt;
    const QJsonValue kind = json.value(QLatin1StringView("kind"));
    message.kind = kind.isString() ? kind.toString() : QStringLiteral("text");
    if (!oneOf(message.kind, {"text", "sticker", "image"}))
        return std::nullopt;
    const QJsonValue sticker = json.value(QLatin1StringView("sticker_id"));
    if (sticker.isString() && sticker.toString().size() <= 40)
        message.stickerId = sticker.toString();
    return message;
}

std::optional<MemberRecord> parseMember(const QJsonObject &json)
{
    const auto user = text(json, "user_id", 36), role = text(json, "role", 12);
    if (!user || !role || !isUuid(*user) || !oneOf(*role, {"owner", "moderator", "member"}))
        return std::nullopt;
    return MemberRecord{user->toLower(), *role};
}

std::optional<ProfileRecord> parseProfile(const QJsonObject &json)
{
    const auto id = text(json, "id", 36), display = text(json, "display_name", 120),
               username = text(json, "username", 60);
    if (!id || !display || !username || !isUuid(*id))
        return std::nullopt;
    return ProfileRecord{id->toLower(), *display, *username};
}

} // namespace katsodi
