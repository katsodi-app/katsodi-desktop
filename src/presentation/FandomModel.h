#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QUrl>
#include <vector>

namespace katsodi {

/**
 * The bundled fandom catalogue (150 entries), filtered by category and by
 * the website's search rules. Posters are files next to the executable and
 * are decoded by the view only when a card is on screen.
 */
class FandomModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY filterChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filterChanged)
    Q_PROPERTY(int total READ total CONSTANT)
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, CategoryRole, ArtRole, ArtWidthRole, ArtHeightRole, InitialsRole };

    FandomModel(const QUrl &artworkBase, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString category() const { return m_category; }
    QString query() const { return m_query; }
    int total() const { return static_cast<int>(m_all.size()); }
    void setCategory(const QString &category);
    void setQuery(const QString &query);
    Q_INVOKABLE QString nameOf(const QString &id) const;
    Q_INVOKABLE QUrl artOf(const QString &id) const;

    bool load(const QByteArray &json);

signals:
    void filterChanged();

private:
    struct Fandom {
        QString id, name, category, art, initials;
        int width = 0, height = 0;
    };
    void refilter();
    std::vector<Fandom> m_all;
    std::vector<int> m_visible;
    QUrl m_artworkBase;
    QString m_category, m_query;
};

} // namespace katsodi
