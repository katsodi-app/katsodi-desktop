#include "presentation/SearchModel.h"

#include "domain/Text.h"
#include "presentation/FandomModel.h"
#include "presentation/RoomListModel.h"

namespace katsodi {

SearchModel::SearchModel(RoomListModel *rooms, FandomModel *fandoms, QObject *parent)
    : QAbstractListModel(parent), m_rooms(rooms), m_fandoms(fandoms)
{
    // The room list reloads in the background; results follow it.
    connect(rooms, &RoomListModel::stateChanged, this, &SearchModel::rebuild);
}

void SearchModel::setQuery(const QString &query)
{
    const QString bounded = query.left(200);
    if (bounded == m_query)
        return;
    m_query = bounded;
    rebuild();
}

void SearchModel::setFilter(const QString &filter)
{
    if (filter == m_filter || (filter != QLatin1StringView("all") && filter != QLatin1StringView("rooms")
                               && filter != QLatin1StringView("fandoms")))
        return;
    m_filter = filter;
    rebuild();
}

void SearchModel::rebuild()
{
    std::vector<Result> results;
    const bool empty = searchText(m_query).isEmpty();
    if (!empty && m_rooms && m_filter != QLatin1StringView("fandoms")) {
        // Search all loaded rooms, not only the ones the catalogue filter shows.
        const RoomListModel &rooms = *m_rooms;
        int added = 0;
        for (const RoomRecord &room : rooms.all()) {
            if (added >= perGroup)
                break;
            if (!matchesSearch(m_query, {room.name, room.description}))
                continue;
            results.push_back({QStringLiteral("room"), room.id, room.name,
                               room.description.isEmpty() ? room.format : room.description, initials(room.name),
                               room.art});
            ++added;
        }
    }
    if (!empty && m_fandoms && m_filter != QLatin1StringView("rooms")) {
        int added = 0;
        for (const auto &fandom : m_fandoms->all()) {
            if (added >= perGroup)
                break;
            if (!matchesSearch(m_query, {fandom.name}))
                continue;
            results.push_back({QStringLiteral("fandom"), fandom.id, fandom.name, fandom.category, fandom.initials,
                               m_fandoms->artOf(fandom.id)});
            ++added;
        }
    }
    beginResetModel();
    m_results = std::move(results);
    endResetModel();
    emit changed();
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= count())
        return {};
    const Result &result = m_results[static_cast<size_t>(index.row())];
    switch (role) {
    case TypeRole: return result.type;
    case IdRole: return result.id;
    case TitleRole: return result.title;
    case SubtitleRole: return result.subtitle;
    case ArtRole: return result.art;
    case InitialsRole: return result.initials;
    default: return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {{TypeRole, "type"}, {IdRole, "itemId"}, {TitleRole, "title"}, {SubtitleRole, "subtitle"},
            {ArtRole, "art"}, {InitialsRole, "initials"}};
}

} // namespace katsodi
