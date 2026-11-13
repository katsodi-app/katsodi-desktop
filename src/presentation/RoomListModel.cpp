#include "presentation/RoomListModel.h"

#include "domain/Text.h"
#include "infrastructure/api/ApiClient.h"

#include <QJsonArray>

namespace katsodi {

RoomListModel::RoomListModel(ApiClient *api, QObject *parent) : QAbstractListModel(parent), m_api(api) {}

void RoomListModel::reload()
{
    const quint64 generation = ++m_generation;
    m_loading = true;
    m_errorKey.clear();
    emit stateChanged();
    m_api->get(QStringLiteral("/api/native/v1/rooms"), this,
               [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation)
            return; // a newer reload answered or will answer
        m_loading = false;
        if (error) {
            m_errorKey = error->messageKey;
            emit stateChanged();
            return;
        }
        apply(body);
    });
}

void RoomListModel::apply(const QJsonObject &body)
{
    std::vector<RoomRecord> rooms;
    const QJsonArray items = body.value(QLatin1StringView("rooms")).toArray();
    for (const QJsonValue &item : items) {
        if (rooms.size() >= 200)
            break;
        if (auto room = parseRoom(item.toObject()); room && !room->closed)
            rooms.push_back(std::move(*room));
    }
    m_all = std::move(rooms);
    m_loading = false;
    m_errorKey.clear();
    refilter();
    emit stateChanged();
}

void RoomListModel::refilter()
{
    std::vector<int> visible;
    for (int i = 0; i < static_cast<int>(m_all.size()); ++i) {
        const RoomRecord &room = m_all[static_cast<size_t>(i)];
        if (!m_format.isEmpty() && room.format != m_format)
            continue;
        if (!m_query.isEmpty() && !matchesSearch(m_query, {room.name, room.description}))
            continue;
        visible.push_back(i);
    }
    beginResetModel();
    m_visible = std::move(visible);
    endResetModel();
}

void RoomListModel::setFormat(const QString &format)
{
    if (format == m_format || (!format.isEmpty() && format != QLatin1StringView("video")
                               && format != QLatin1StringView("music")))
        return;
    m_format = format;
    refilter();
    emit filterChanged();
}

void RoomListModel::setQuery(const QString &query)
{
    const QString bounded = query.left(200);
    if (bounded == m_query)
        return;
    m_query = bounded;
    refilter();
    emit filterChanged();
}

void RoomListModel::createShared(const QVariantMap &input)
{
    QJsonObject body;
    for (const char *key : {"name", "description", "format", "visibility", "url", "art", "fandomId"}) {
        const QString value = input.value(QLatin1StringView(key)).toString();
        if (!value.isEmpty() || QLatin1StringView(key) == QLatin1StringView("description")
            || QLatin1StringView(key) == QLatin1StringView("url"))
            body.insert(QLatin1StringView(key), value.trimmed());
    }
    if (!body.contains(QLatin1StringView("art")))
        body.insert(QStringLiteral("art"), QStringLiteral("orbit"));
    // Not idempotent: no automatic resend after an ambiguous failure.
    m_api->post(QStringLiteral("/api/native/v1/rooms"), body, this,
                [this](const QJsonObject &reply, const ApiError *error) {
        if (error) {
            emit createFailed(error->messageKey);
            return;
        }
        const QString id = reply.value(QLatin1StringView("id")).toString();
        if (!isUuid(id)) {
            emit createFailed(QStringLiteral("errorServer"));
            return;
        }
        emit created(id.toLower(), reply.value(QLatin1StringView("importFailed")).toBool());
        reload();
    });
}

QVariantMap RoomListModel::room(const QString &id) const
{
    for (const RoomRecord &room : m_all)
        if (room.id == id)
            return {{"roomId", room.id}, {"name", room.name}, {"description", room.description},
                    {"format", room.format}, {"visibility", room.visibility}, {"art", room.art},
                    {"fandomId", room.fandomId}};
    return {};
}

QVariantList RoomListModel::roomsOfFandom(const QString &fandomId) const
{
    QVariantList out;
    if (fandomId.isEmpty())
        return out;
    for (const RoomRecord &record : m_all) {
        if (record.fandomId != fandomId)
            continue;
        out.append(room(record.id));
        if (out.size() >= 60)
            break;
    }
    return out;
}

int RoomListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_visible.size());
}

QVariant RoomListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_visible.size()))
        return {};
    const RoomRecord &room = m_all[static_cast<size_t>(m_visible[static_cast<size_t>(index.row())])];
    switch (role) {
    case IdRole: return room.id;
    case NameRole: return room.name;
    case DescriptionRole: return room.description;
    case FormatRole: return room.format;
    case VisibilityRole: return room.visibility;
    case ArtRole: return room.art;
    case FandomRole: return room.fandomId;
    case CreatedRole: return room.createdAt;
    default: return {};
    }
}

QHash<int, QByteArray> RoomListModel::roleNames() const
{
    return {{IdRole, "roomId"}, {NameRole, "name"}, {DescriptionRole, "description"},
            {FormatRole, "format"}, {VisibilityRole, "visibility"}, {ArtRole, "art"},
            {FandomRole, "fandomId"}, {CreatedRole, "createdAt"}};
}

} // namespace katsodi
