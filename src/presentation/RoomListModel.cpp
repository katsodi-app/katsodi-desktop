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
