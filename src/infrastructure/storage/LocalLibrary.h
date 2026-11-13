#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QSet>
#include <QSqlDatabase>
#include <QVariantMap>
#include <vector>

namespace katsodi {

/**
 * The application's own local data in SQLite (not browser storage): room
 * drafts, bookmarked shared rooms and followed fandoms. Same meaning and
 * limits as the website's local workspace (src/lib/rooms.ts,
 * workspace-provider.tsx): at most 100 drafts, 500 bookmarks, 200 fandoms.
 *
 * Drafts are local only: they are never sent anywhere and are not shared
 * rooms. The model lists drafts, newest first.
 *
 * Schema versions are kept in PRAGMA user_version and migrated in a
 * transaction. A database that cannot be opened or fails its integrity
 * check is moved aside (kept as a backup, never deleted) and a new one
 * is created; `recovered` tells the UI so it can say what happened.
 */
class LocalLibrary : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int version READ version NOTIFY changed) // bump for QML bindings
    Q_PROPERTY(bool recovered READ recovered CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, DescriptionRole, FormatRole, VisibilityRole, UrlRole, ArtRole, FandomRole, CreatedRole };

    static constexpr int maxDrafts = 100, maxSaved = 500, maxFollowed = 200, schemaVersion = 1;

    explicit LocalLibrary(const QString &file, QObject *parent = nullptr);
    ~LocalLibrary() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_drafts.size()); }
    int version() const { return m_version; }
    bool recovered() const { return m_recovered; }
    bool available() const { return m_db.isOpen(); }

    /** Validates like the website; returns "" on success or a string id. */
    Q_INVOKABLE QString createDraft(const QVariantMap &input);
    Q_INVOKABLE void deleteDraft(const QString &id);
    Q_INVOKABLE QVariantMap draft(const QString &id) const;
    Q_INVOKABLE bool isSaved(const QString &roomId) const { return m_saved.contains(roomId); }
    Q_INVOKABLE bool toggleSaved(const QString &roomId);
    Q_INVOKABLE bool isFollowed(const QString &fandomId) const { return m_followed.contains(fandomId); }
    Q_INVOKABLE bool toggleFollowed(const QString &fandomId);
    Q_INVOKABLE QStringList savedIds() const { return QStringList(m_saved.begin(), m_saved.end()); }
    Q_INVOKABLE int followedCount() const { return static_cast<int>(m_followed.size()); }

    /** Validation shared with tests: "" when valid, else a string id. */
    static QString validate(const QVariantMap &input);

signals:
    void changed();

private:
    struct Draft {
        QString id, name, description, format, visibility, url, art, fandomId;
        QDateTime createdAt;
    };
    bool open(const QString &file);
    bool migrate();
    void load();
    void bump();

    QString m_connection;
    QSqlDatabase m_db;
    std::vector<Draft> m_drafts;
    QSet<QString> m_saved, m_followed;
    bool m_recovered = false;
    int m_version = 0;
};

} // namespace katsodi
