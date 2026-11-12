#pragma once
#include "domain/Records.h"

#include <QAbstractListModel>
#include <QHash>
#include <vector>

namespace katsodi {

/**
 * The room's message window, oldest first. Updates are applied as precise
 * row insertions and removals, so the view keeps its scroll position and
 * delegates for unchanged messages are not rebuilt.
 *
 * Full snapshot: the window is replaced (rows matched by id).
 * Delta snapshot: `ids` is the current window; rows not in it are removed
 * (deleted or scrolled out) and `fresh` rows are inserted in order.
 */
class ChatModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { IdRole = Qt::UserRole + 1, SenderRole, AuthorRole, BodyRole, KindRole, StickerRole, TimeRole, GroupedRole, OwnRole };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    QString lastId() const { return m_rows.empty() ? QString() : m_rows.back().id; }

    void setViewer(const QString &userId) { m_viewer = userId; }
    void setNames(QHash<QString, QString> names);
    void replaceAll(std::vector<MessageRecord> messages);
    void applyDelta(const QStringList &ids, std::vector<MessageRecord> fresh);
    void clear();

signals:
    void countChanged();

private:
    void syncTo(const std::vector<MessageRecord> &target);
    std::vector<MessageRecord> m_rows;
    QHash<QString, QString> m_names;
    QString m_viewer;
};

} // namespace katsodi
