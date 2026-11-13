#include "presentation/RoomController.h"

#include "application/Session.h"
#include "domain/Text.h"
#include "infrastructure/api/ApiClient.h"
#include "presentation/Preferences.h"

#include <QJsonArray>
#include <QUrl>
#include <QVariantMap>
#include <cmath>

namespace katsodi {

RoomController::RoomController(ApiClient *api, Session *session, Preferences *preferences, QObject *parent)
    : QObject(parent), m_api(api), m_session(session), m_preferences(preferences)
{
    m_poll.setSingleShot(true);
    connect(&m_poll, &QTimer::timeout, this, [this] { load(false); });
    // The website pings every 30 s while a member has the room open.
    m_occupancy.setInterval(30000);
    connect(&m_occupancy, &QTimer::timeout, this, &RoomController::ping);
    // A different account must not keep the previous account's room state.
    connect(session, &Session::changed, this, [this] {
        if (!m_roomId.isEmpty() && m_session->userId() != m_viewer)
            open(m_roomId);
    });
}

void RoomController::open(const QString &roomId)
{
    close();
    if (!isUuid(roomId)) {
        m_errorKey = QStringLiteral("roomNotFound");
        emit stateChanged();
        return;
    }
    m_roomId = roomId.toLower();
    m_viewer = m_session->userId();
    m_chat.setViewer(m_viewer);
    emit roomChanged();
    load(true);
}

void RoomController::close()
{
    ++m_generation; // drops every answer still on its way
    m_poll.stop();
    m_occupancy.stop();
    m_inFlight = false;
    m_roomId.clear();
    m_room = RoomRecord{};
    m_members.clear();
    m_queue.clear();
    m_playback.clear();
    m_playbackPosition = 0;
    m_queueing = false;
    m_joined = false;
    m_loading = false;
    m_sending = false;
    m_joining = false;
    m_errorKey.clear();
    m_actionErrorKey.clear();
    m_role.clear();
    m_chat.clear();
    emit roomChanged();
    emit stateChanged();
    emit sendChanged();
}

void RoomController::load(bool full)
{
    if (m_roomId.isEmpty() || m_inFlight)
        return;
    m_poll.stop();
    m_inFlight = true;
    if (full) {
        m_loading = m_room.id.isEmpty();
        emit stateChanged();
    }
    QString path = QStringLiteral("/api/native/v1/rooms/") + m_roomId;
    const QString after = m_chat.lastId();
    if (!full && !after.isEmpty())
        path += QStringLiteral("?after=") + after;
    const quint64 generation = m_generation;
    m_api->get(path, this, [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation)
            return;
        m_inFlight = false;
        m_loading = false;
        if (error) {
            if (error->code == QLatin1StringView("not_found")) {
                // Missing, hidden and lost access look the same on purpose.
                m_room = RoomRecord{};
                m_joined = false;
                m_members.clear();
                m_chat.clear();
                m_occupancy.stop();
                m_errorKey = QStringLiteral("roomNotFound");
                emit stateChanged();
                return; // no more polling for a room we cannot see
            }
            m_errorKey = error->messageKey;
            emit stateChanged();
            schedule();
            return;
        }
        applySnapshot(body);
        schedule();
    });
}

void RoomController::applySnapshot(const QJsonObject &body)
{
    auto room = parseRoom(body.value(QLatin1StringView("room")).toObject());
    if (!room) {
        m_errorKey = QStringLiteral("errorServer");
        emit stateChanged();
        return;
    }
    if (!m_roomId.isEmpty() && room->id != m_roomId)
        return; // an answer for another room is never shown here
    m_room = *room;
    m_errorKey = room->closed ? QStringLiteral("roomClosed") : QString();
    m_joined = body.value(QLatin1StringView("joined")).toBool(false);

    QHash<QString, QString> names;
    for (const QJsonValue &item : body.value(QLatin1StringView("profiles")).toArray())
        if (auto profile = parseProfile(item.toObject()))
            names.insert(profile->id, profile->displayName);
    m_chat.setNames(names);

    m_members.clear();
    m_role.clear();
    for (const QJsonValue &item : body.value(QLatin1StringView("members")).toArray()) {
        auto member = parseMember(item.toObject());
        if (!member)
            continue;
        if (member->userId == m_viewer)
            m_role = member->role;
        m_members.append(QVariantMap{{QStringLiteral("id"), member->userId},
                                     {QStringLiteral("name"), names.value(member->userId, QStringLiteral("Katsodi"))},
                                     {QStringLiteral("role"), member->role},
                                     {QStringLiteral("initials"), initials(names.value(member->userId))}});
        if (m_members.size() >= 200)
            break;
    }

    // Queue (links waiting to play) and the shared timeline, when present.
    m_queue.clear();
    for (const QJsonValue &item : body.value(QLatin1StringView("queue")).toArray()) {
        const QJsonObject entry = item.toObject();
        const QString id = entry.value(QLatin1StringView("id")).toString();
        const QString url = entry.value(QLatin1StringView("url")).toString();
        if (!isUuid(id) || url.isEmpty() || url.size() > 2048)
            continue;
        const QString addedBy = entry.value(QLatin1StringView("added_by")).toString();
        m_queue.append(QVariantMap{{QStringLiteral("id"), id},
                                   {QStringLiteral("url"), url},
                                   {QStringLiteral("host"), QUrl(url).host()},
                                   {QStringLiteral("addedBy"), names.value(addedBy.toLower())}});
        if (m_queue.size() >= 300)
            break;
    }
    const QJsonObject playback = body.value(QLatin1StringView("playback")).toObject();
    const QString state = playback.value(QLatin1StringView("state")).toString();
    m_playback = state == QLatin1StringView("playing") || state == QLatin1StringView("paused") ? state : QString();
    m_playbackPosition = 0;
    if (!m_playback.isEmpty()) {
        const double position = playback.value(QLatin1StringView("position")).toDouble();
        const auto updated = parseTimestamp(playback.value(QLatin1StringView("updated_at")).toString());
        const auto server = parseTimestamp(body.value(QLatin1StringView("serverTime")).toString());
        double elapsed = 0;
        // Extrapolate with the server's own clock, never the local one.
        if (m_playback == QLatin1StringView("playing") && updated && server)
            elapsed = std::max<qint64>(0, updated->msecsTo(*server)) / 1000.0;
        if (std::isfinite(position) && position >= 0 && position <= 172800)
            m_playbackPosition = position + elapsed;
    }

    std::vector<MessageRecord> messages;
    for (const QJsonValue &item : body.value(QLatin1StringView("messages")).toArray())
        if (auto message = parseMessage(item.toObject()))
            messages.push_back(std::move(*message));
    if (body.contains(QLatin1StringView("messageIds"))) {
        QStringList ids;
        for (const QJsonValue &id : body.value(QLatin1StringView("messageIds")).toArray())
            if (isUuid(id.toString()))
                ids.append(id.toString().toLower());
        m_chat.applyDelta(ids, std::move(messages));
    } else {
        m_chat.replaceAll(std::move(messages));
    }

    if (m_joined && !m_room.closed && !m_occupancy.isActive()) {
        ping();
        m_occupancy.start();
    } else if (!m_joined || m_room.closed) {
        m_occupancy.stop();
    }
    emit stateChanged();
}

void RoomController::schedule()
{
    if (m_roomId.isEmpty() || m_room.closed)
        return;
    // Same cadence as the website's polling client; slower in low-resource mode.
    m_poll.start(m_preferences->lowResources() ? 6000 : 3000);
}

void RoomController::ping()
{
    if (m_roomId.isEmpty() || !m_joined)
        return;
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/occupancy").arg(m_roomId), {}, this,
                [](const QJsonObject &, const ApiError *) {});
}

void RoomController::join()
{
    if (m_roomId.isEmpty() || m_joining || m_joined)
        return;
    m_joining = true;
    m_actionErrorKey.clear();
    emit sendChanged();
    const quint64 generation = m_generation;
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/join").arg(m_roomId), {}, this,
                [this, generation](const QJsonObject &, const ApiError *error) {
        if (generation != m_generation)
            return;
        m_joining = false;
        m_actionErrorKey = error ? error->messageKey : QString();
        emit sendChanged();
        load(true);
    });
}

bool RoomController::addToQueue(const QString &url)
{
    if (m_roomId.isEmpty() || m_queueing || !m_joined || !isAcceptableSource(url))
        return false;
    m_queueing = true;
    m_actionErrorKey.clear();
    emit sendChanged();
    const quint64 generation = m_generation;
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/queue").arg(m_roomId),
                QJsonObject{{QStringLiteral("url"), url.trimmed()}}, this,
                [this, generation](const QJsonObject &, const ApiError *error) {
        if (generation != m_generation)
            return;
        m_queueing = false;
        m_actionErrorKey = error ? error->messageKey : QString();
        emit sendChanged();
        if (!error) {
            emit queued();
            load(false);
        }
    });
    return true;
}

bool RoomController::send(const QString &text)
{
    if (m_roomId.isEmpty() || m_sending || !m_joined || !isValidMessage(text))
        return false;
    m_sending = true;
    m_actionErrorKey.clear();
    emit sendChanged();
    const quint64 generation = m_generation;
    // Never resent automatically: an ambiguous failure could have succeeded.
    m_api->post(QStringLiteral("/api/native/v1/rooms/%1/messages").arg(m_roomId),
                QJsonObject{{QStringLiteral("body"), text.trimmed()}}, this,
                [this, generation](const QJsonObject &, const ApiError *error) {
        if (generation != m_generation)
            return;
        m_sending = false;
        if (error) {
            m_actionErrorKey = error->code == QLatin1StringView("rate_limited")
                                   ? error->messageKey
                                   : QStringLiteral("roomSendFailed");
            emit sendChanged();
            return;
        }
        emit sendChanged();
        emit sent();
        load(false);
    });
    return true;
}

} // namespace katsodi
