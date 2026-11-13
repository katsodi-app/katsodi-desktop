#pragma once
#include "domain/Records.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <vector>

namespace katsodi {

class ApiClient;

/**
 * Open shared rooms from GET /api/native/v1/rooms, filtered locally by
 * format and search text. Guests receive public rooms only (server rule).
 */
class RoomListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY stateChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY filterChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filterChanged)
    Q_PROPERTY(int total READ total NOTIFY stateChanged)
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, DescriptionRole, FormatRole, VisibilityRole, ArtRole, FandomRole, CreatedRole };

    RoomListModel(ApiClient *api, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_loading; }
    QString errorKey() const { return m_errorKey; }
    QString format() const { return m_format; }
    QString query() const { return m_query; }
    int total() const { return static_cast<int>(m_all.size()); }
    void setFormat(const QString &format);
    void setQuery(const QString &query);

    Q_INVOKABLE void reload();
    /** Creates a shared room on the server (signed-in accounts). */
    Q_INVOKABLE void createShared(const QVariantMap &input);
    /** A loaded room by id as a map for QML, or empty. */
    Q_INVOKABLE QVariantMap room(const QString &id) const;
    /** The loaded rooms of one fandom, as room() maps (at most 60). */
    Q_INVOKABLE QVariantList roomsOfFandom(const QString &fandomId) const;
    const std::vector<RoomRecord> &all() const { return m_all; }
    /** Applies a response body (public for tests). */
    void apply(const QJsonObject &body);

signals:
    void stateChanged();
    void filterChanged();
    void created(const QString &roomId, bool importFailed);
    void createFailed(const QString &messageKey);

private:
    void refilter();
    ApiClient *m_api;
    std::vector<RoomRecord> m_all;
    std::vector<int> m_visible;
    bool m_loading = false;
    QString m_errorKey, m_format, m_query;
    quint64 m_generation = 0;
};

} // namespace katsodi
