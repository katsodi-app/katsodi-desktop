#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

namespace katsodi {

class ApiClient;
class RoomController;

/**
 * The music library, as on the website: the all-time chart, the providers'
 * weekly charts and the popular / trending collections, with a text filter;
 * "Listen in a room" puts a track into the open music room you are a member
 * of, or creates a private music room for it (the website's operation).
 */
class MusicController : public QObject {
    Q_OBJECT
    /** [{ id, label, provider, checked, stale }] */
    Q_PROPERTY(QVariantList shelves READ shelves NOTIFY changed)
    Q_PROPERTY(QString shelf READ shelf WRITE setShelf NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    /** The current shelf's tracks matching the query:
     *  [{ rank, title, artists, url, cover, plays, duration, kind }] */
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    /** The open room takes tracks directly (a music room you are in). */
    Q_PROPERTY(bool roomTakesTracks READ roomTakesTracks NOTIFY changed)
public:
    MusicController(ApiClient *api, RoomController *room, QObject *parent = nullptr);

    QVariantList shelves() const { return m_shelfList; }
    QString shelf() const { return m_shelf; }
    void setShelf(const QString &id);
    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QVariantList tracks() const { return m_visible; }
    bool loading() const { return m_loading; }
    bool busy() const { return m_busy; }
    QString errorKey() const { return m_errorKey; }
    bool roomTakesTracks() const;

    Q_INVOKABLE void reload();
    /** Listen to a track in a room; openRoom(id) follows. */
    Q_INVOKABLE void listen(const QString &url, const QString &title);

    /** Applies a library answer (public for tests). */
    void applyLibrary(const QJsonObject &body);

signals:
    void changed();
    void openRoom(const QString &roomId);

private:
    void refilter();

    ApiClient *m_api;
    RoomController *m_room;
    QVariantList m_shelfList, m_visible;
    QHash<QString, QVariantList> m_shelfTracks;
    QString m_shelf = QStringLiteral("all-time"), m_query, m_errorKey;
    bool m_loading = false, m_busy = false, m_loaded = false;
};

} // namespace katsodi
