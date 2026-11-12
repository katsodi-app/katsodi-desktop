#include "presentation/ChatModel.h"

#include <QSet>

namespace katsodi {

ChatModel::ChatModel(QObject *parent) : QAbstractListModel(parent) {}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size()))
        return {};
    const size_t row = static_cast<size_t>(index.row());
    const MessageRecord &message = m_rows[row];
    switch (role) {
    case IdRole: return message.id;
    case SenderRole: return message.senderId;
    case AuthorRole: return m_names.value(message.senderId, QStringLiteral("Katsodi"));
    case BodyRole: return message.body;
    case KindRole: return message.kind;
    case StickerRole: return message.stickerId;
    case TimeRole: return message.createdAt.toLocalTime();
    case GroupedRole:
        // Same author within five minutes: shown without a repeated header.
        return row > 0 && m_rows[row - 1].senderId == message.senderId
               && m_rows[row - 1].createdAt.secsTo(message.createdAt) < 300;
    case OwnRole: return !m_viewer.isEmpty() && message.senderId == m_viewer;
    default: return {};
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {{IdRole, "messageId"}, {SenderRole, "senderId"}, {AuthorRole, "author"},
            {BodyRole, "body"}, {KindRole, "kind"}, {StickerRole, "stickerId"},
            {TimeRole, "time"}, {GroupedRole, "grouped"}, {OwnRole, "own"}};
}

void ChatModel::setNames(QHash<QString, QString> names)
{
    if (names == m_names)
        return;
    m_names = std::move(names);
    if (!m_rows.empty())
        emit dataChanged(index(0), index(count() - 1), {AuthorRole});
}

void ChatModel::clear()
{
    if (m_rows.empty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

void ChatModel::replaceAll(std::vector<MessageRecord> messages)
{
    syncTo(messages);
}

void ChatModel::applyDelta(const QStringList &ids, std::vector<MessageRecord> fresh)
{
    QHash<QString, const MessageRecord *> known;
    for (const MessageRecord &row : m_rows)
        known.insert(row.id, &row);
    QHash<QString, MessageRecord *> incoming;
    for (MessageRecord &row : fresh)
        incoming.insert(row.id, &row);
    std::vector<MessageRecord> target;
    target.reserve(static_cast<size_t>(ids.size()));
    for (const QString &id : ids) {
        if (auto *row = incoming.value(id))
            target.push_back(*row);
        else if (auto *row2 = known.value(id))
            target.push_back(*row2);
        // An id neither known nor sent is skipped; the next full snapshot fills it.
    }
    syncTo(target);
}

void ChatModel::syncTo(const std::vector<MessageRecord> &target)
{
    QSet<QString> wanted;
    for (const MessageRecord &row : target)
        wanted.insert(row.id);
    // 1. Remove rows that left the window, from the end to keep indices valid.
    for (int i = count() - 1; i >= 0; --i) {
        if (!wanted.contains(m_rows[static_cast<size_t>(i)].id)) {
            beginRemoveRows({}, i, i);
            m_rows.erase(m_rows.begin() + i);
            endRemoveRows();
        }
    }
    // 2. Walk the target order, inserting rows that are new at their place.
    for (size_t i = 0; i < target.size(); ++i) {
        if (i < m_rows.size() && m_rows[i].id == target[i].id)
            continue;
        // Row exists later (order changed): move by removal + insertion.
        for (size_t j = i + 1; j < m_rows.size(); ++j)
            if (m_rows[j].id == target[i].id) {
                beginRemoveRows({}, static_cast<int>(j), static_cast<int>(j));
                m_rows.erase(m_rows.begin() + static_cast<qsizetype>(j));
                endRemoveRows();
                break;
            }
        beginInsertRows({}, static_cast<int>(i), static_cast<int>(i));
        m_rows.insert(m_rows.begin() + static_cast<qsizetype>(i), target[i]);
        endInsertRows();
    }
    // Grouping of the row after an insertion can change.
    if (!m_rows.empty())
        emit dataChanged(index(0), index(count() - 1), {GroupedRole, OwnRole});
    emit countChanged();
}

} // namespace katsodi
