#pragma once
#include <QAbstractListModel>
#include <QPointer>
#include <vector>

namespace katsodi {

class FandomModel;
class RoomListModel;

/**
 * Quick search (Ctrl+K) over what the app already has: open shared rooms
 * and the bundled fandom catalogue, with the website's matching rules.
 * Results are grouped (rooms first) and limited to 8 per group. Nothing is
 * sent to the server while typing.
 */
class SearchModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed) // all | rooms | fandoms
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    enum Role { TypeRole = Qt::UserRole + 1, IdRole, TitleRole, SubtitleRole, ArtRole, InitialsRole };

    SearchModel(RoomListModel *rooms, FandomModel *fandoms, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    QString filter() const { return m_filter; }
    int count() const { return static_cast<int>(m_results.size()); }
    void setQuery(const QString &query);
    void setFilter(const QString &filter);

    static constexpr int perGroup = 8;

signals:
    void changed();

private:
    struct Result {
        QString type, id, title, subtitle, initials;
        QVariant art;
    };
    void rebuild();

    QPointer<RoomListModel> m_rooms;
    QPointer<FandomModel> m_fandoms;
    QString m_query, m_filter = QStringLiteral("all");
    std::vector<Result> m_results;
};

} // namespace katsodi
