#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace katsodi {

/**
 * Records received from the native API, validated field by field. A record
 * that breaks the contract (unknown enum, bad id, oversized text) is
 * rejected as a whole rather than shown half-filled.
 */
struct RoomRecord {
    QString id;
    QString ownerId;
    QString name;
    QString description;
    QString format;     // video | music
    QString visibility; // public | private
    QString url;
    QString art;        // orbit | wave | moon | sun
    QString fandomId;   // empty: no fandom ("Other")
    bool closed = false;
    QDateTime createdAt;
};

struct MessageRecord {
    QString id;
    QString senderId;
    QString body;
    QString kind; // text | sticker | image
    QString stickerId;
    QDateTime createdAt;
};

struct MemberRecord {
    QString userId;
    QString role; // owner | moderator | member
};

struct ProfileRecord {
    QString id;
    QString displayName;
    QString username;
};

std::optional<RoomRecord> parseRoom(const QJsonObject &json);
std::optional<MessageRecord> parseMessage(const QJsonObject &json);
std::optional<MemberRecord> parseMember(const QJsonObject &json);
std::optional<ProfileRecord> parseProfile(const QJsonObject &json);

/** ISO 8601 with offset, as PostgreSQL/JSON sends it; invalid → nullopt. */
std::optional<QDateTime> parseTimestamp(const QString &value);

} // namespace katsodi
