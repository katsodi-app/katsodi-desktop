#include "infrastructure/storage/LocalLibrary.h"

#include "domain/Text.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace katsodi {
namespace {

const QStringList kArts{QStringLiteral("orbit"), QStringLiteral("wave"), QStringLiteral("moon"), QStringLiteral("sun")};

bool exec(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    return query.exec(sql);
}

} // namespace

LocalLibrary::LocalLibrary(const QString &file, QObject *parent)
    : QAbstractListModel(parent),
      m_connection(QStringLiteral("katsodi-local-%1").arg(reinterpret_cast<quintptr>(this)))
{
    if (!open(file)) {
        // Keep the unreadable file as a backup and start a fresh database.
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connection);
        if (QFile::exists(file)) {
            QFile::rename(file, file + QStringLiteral(".corrupt-")
                                    + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmss")));
            m_recovered = true;
        }
        open(file);
    }
    load();
}

LocalLibrary::~LocalLibrary()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}

bool LocalLibrary::open(const QString &file)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    m_db.setDatabaseName(file);
    if (!m_db.open())
        return false;
    QSqlQuery check(m_db);
    if (!check.exec(QStringLiteral("PRAGMA quick_check")) || !check.next()
        || check.value(0).toString() != QLatin1StringView("ok"))
        return false;
    exec(m_db, QStringLiteral("PRAGMA foreign_keys = ON"));
    exec(m_db, QStringLiteral("PRAGMA journal_mode = WAL"));
    exec(m_db, QStringLiteral("PRAGMA synchronous = NORMAL"));
    return migrate();
}

bool LocalLibrary::migrate()
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return false;
    const int current = query.value(0).toInt();
    if (current > schemaVersion)
        return false; // written by a newer app: do not guess, do not overwrite
    if (current == schemaVersion)
        return true;
    if (!m_db.transaction())
        return false;
    const bool ok =
        exec(m_db, QStringLiteral("CREATE TABLE IF NOT EXISTS drafts ("
                                  " id TEXT PRIMARY KEY NOT NULL,"
                                  " name TEXT NOT NULL CHECK (length(name) BETWEEN 3 AND 80),"
                                  " description TEXT NOT NULL CHECK (length(description) <= 500),"
                                  " format TEXT NOT NULL CHECK (format IN ('video','music')),"
                                  " visibility TEXT NOT NULL CHECK (visibility IN ('public','private')),"
                                  " url TEXT NOT NULL CHECK (length(url) <= 2048),"
                                  " art TEXT NOT NULL CHECK (art IN ('orbit','wave','moon','sun')),"
                                  " fandom_id TEXT,"
                                  " created_at TEXT NOT NULL)"))
        && exec(m_db, QStringLiteral("CREATE TABLE IF NOT EXISTS saved_rooms ("
                                     " room_id TEXT PRIMARY KEY NOT NULL, saved_at TEXT NOT NULL)"))
        && exec(m_db, QStringLiteral("CREATE TABLE IF NOT EXISTS followed_fandoms ("
                                     " fandom_id TEXT PRIMARY KEY NOT NULL, followed_at TEXT NOT NULL)"))
        && exec(m_db, QStringLiteral("PRAGMA user_version = %1").arg(schemaVersion));
    if (!ok) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

void LocalLibrary::load()
{
    if (!m_db.isOpen())
        return;
    std::vector<Draft> drafts;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("SELECT id, name, description, format, visibility, url, art, fandom_id, created_at"
                              " FROM drafts ORDER BY created_at DESC LIMIT %1").arg(maxDrafts));
    while (query.next()) {
        Draft draft{query.value(0).toString(), query.value(1).toString(), query.value(2).toString(),
                    query.value(3).toString(), query.value(4).toString(), query.value(5).toString(),
                    query.value(6).toString(), query.value(7).toString(),
                    QDateTime::fromString(query.value(8).toString(), Qt::ISODate)};
        // Rows are validated again on read; a broken row is skipped, not shown.
        if (isUuid(draft.id) && validate({{"name", draft.name}, {"description", draft.description},
                                          {"format", draft.format}, {"visibility", draft.visibility},
                                          {"url", draft.url}, {"art", draft.art}}).isEmpty())
            drafts.push_back(std::move(draft));
    }
    QSet<QString> saved, followed;
    query.exec(QStringLiteral("SELECT room_id FROM saved_rooms LIMIT %1").arg(maxSaved));
    while (query.next())
        if (isUuid(query.value(0).toString()))
            saved.insert(query.value(0).toString().toLower());
    query.exec(QStringLiteral("SELECT fandom_id FROM followed_fandoms LIMIT %1").arg(maxFollowed));
    static const QRegularExpression fandomId(QStringLiteral(R"(^[a-z]+-\d{1,3}$)"));
    while (query.next())
        if (fandomId.match(query.value(0).toString()).hasMatch())
            followed.insert(query.value(0).toString());
    beginResetModel();
    m_drafts = std::move(drafts);
    m_saved = std::move(saved);
    m_followed = std::move(followed);
    endResetModel();
    bump();
}

void LocalLibrary::bump()
{
    ++m_version;
    emit changed();
}

QString LocalLibrary::validate(const QVariantMap &input)
{
    const QString name = input.value(QStringLiteral("name")).toString().trimmed();
    const QString description = input.value(QStringLiteral("description")).toString().trimmed();
    const QString format = input.value(QStringLiteral("format")).toString();
    const QString visibility = input.value(QStringLiteral("visibility")).toString();
    const QString url = input.value(QStringLiteral("url")).toString().trimmed();
    if (name.size() < 3 || name.size() > 80)
        return QStringLiteral("createNameInvalid");
    if (description.size() > 500)
        return QStringLiteral("createDescriptionInvalid");
    if (format != QLatin1StringView("video") && format != QLatin1StringView("music"))
        return QStringLiteral("errorInvalid");
    if (visibility != QLatin1StringView("public") && visibility != QLatin1StringView("private"))
        return QStringLiteral("errorInvalid");
    if (!url.isEmpty() && !isAcceptableSource(url))
        return QStringLiteral("createUrlInvalid");
    const QString art = input.value(QStringLiteral("art"), QStringLiteral("orbit")).toString();
    if (!kArts.contains(art))
        return QStringLiteral("errorInvalid");
    return {};
}

QString LocalLibrary::createDraft(const QVariantMap &input)
{
    if (!m_db.isOpen())
        return QStringLiteral("libraryUnavailable");
    if (count() >= maxDrafts)
        return QStringLiteral("draftsFull");
    const QString error = validate(input);
    if (!error.isEmpty())
        return error;
    Draft draft;
    draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.name = input.value(QStringLiteral("name")).toString().trimmed();
    draft.description = input.value(QStringLiteral("description")).toString().trimmed();
    draft.format = input.value(QStringLiteral("format")).toString();
    draft.visibility = input.value(QStringLiteral("visibility")).toString();
    draft.url = input.value(QStringLiteral("url")).toString().trimmed();
    draft.art = input.value(QStringLiteral("art"), kArts.at(QRandomGenerator::global()->bounded(4))).toString();
    draft.fandomId = input.value(QStringLiteral("fandomId")).toString().left(80);
    draft.createdAt = QDateTime::currentDateTimeUtc();
    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO drafts (id, name, description, format, visibility, url, art, fandom_id, created_at)"
                                  " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const QVariant &value : {QVariant(draft.id), QVariant(draft.name), QVariant(draft.description),
                                  QVariant(draft.format), QVariant(draft.visibility), QVariant(draft.url),
                                  QVariant(draft.art),
                                  draft.fandomId.isEmpty() ? QVariant() : QVariant(draft.fandomId),
                                  QVariant(draft.createdAt.toString(Qt::ISODateWithMs))})
        insert.addBindValue(value);
    if (!insert.exec())
        return QStringLiteral("libraryUnavailable");
    beginInsertRows({}, 0, 0);
    m_drafts.insert(m_drafts.begin(), draft);
    endInsertRows();
    bump();
    return {};
}

void LocalLibrary::deleteDraft(const QString &id)
{
    for (size_t i = 0; i < m_drafts.size(); ++i) {
        if (m_drafts[i].id != id)
            continue;
        QSqlQuery remove(m_db);
        remove.prepare(QStringLiteral("DELETE FROM drafts WHERE id = ?"));
        remove.addBindValue(id);
        if (!remove.exec())
            return;
        beginRemoveRows({}, static_cast<int>(i), static_cast<int>(i));
        m_drafts.erase(m_drafts.begin() + static_cast<qsizetype>(i));
        endRemoveRows();
        bump();
        return;
    }
}

QVariantMap LocalLibrary::draft(const QString &id) const
{
    for (const Draft &draft : m_drafts)
        if (draft.id == id)
            return {{"id", draft.id}, {"name", draft.name}, {"description", draft.description},
                    {"format", draft.format}, {"visibility", draft.visibility}, {"url", draft.url},
                    {"art", draft.art}, {"fandomId", draft.fandomId}, {"createdAt", draft.createdAt}};
    return {};
}

bool LocalLibrary::toggleSaved(const QString &roomId)
{
    const QString id = roomId.toLower();
    if (!isUuid(id) || !m_db.isOpen())
        return false;
    QSqlQuery query(m_db);
    if (m_saved.contains(id)) {
        query.prepare(QStringLiteral("DELETE FROM saved_rooms WHERE room_id = ?"));
        query.addBindValue(id);
        if (query.exec())
            m_saved.remove(id);
    } else {
        if (m_saved.size() >= maxSaved)
            return false;
        query.prepare(QStringLiteral("INSERT OR IGNORE INTO saved_rooms (room_id, saved_at) VALUES (?, ?)"));
        query.addBindValue(id);
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        if (query.exec())
            m_saved.insert(id);
    }
    bump();
    return m_saved.contains(id);
}

bool LocalLibrary::toggleFollowed(const QString &fandomId)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^[a-z]+-\d{1,3}$)"));
    if (!pattern.match(fandomId).hasMatch() || !m_db.isOpen())
        return false;
    QSqlQuery query(m_db);
    if (m_followed.contains(fandomId)) {
        query.prepare(QStringLiteral("DELETE FROM followed_fandoms WHERE fandom_id = ?"));
        query.addBindValue(fandomId);
        if (query.exec())
            m_followed.remove(fandomId);
    } else {
        if (m_followed.size() >= maxFollowed)
            return false;
        query.prepare(QStringLiteral("INSERT OR IGNORE INTO followed_fandoms (fandom_id, followed_at) VALUES (?, ?)"));
        query.addBindValue(fandomId);
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        if (query.exec())
            m_followed.insert(fandomId);
    }
    bump();
    return m_followed.contains(fandomId);
}

int LocalLibrary::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant LocalLibrary::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= count())
        return {};
    const Draft &draft = m_drafts[static_cast<size_t>(index.row())];
    switch (role) {
    case IdRole: return draft.id;
    case NameRole: return draft.name;
    case DescriptionRole: return draft.description;
    case FormatRole: return draft.format;
    case VisibilityRole: return draft.visibility;
    case UrlRole: return draft.url;
    case ArtRole: return draft.art;
    case FandomRole: return draft.fandomId;
    case CreatedRole: return draft.createdAt;
    default: return {};
    }
}

QHash<int, QByteArray> LocalLibrary::roleNames() const
{
    return {{IdRole, "draftId"}, {NameRole, "name"}, {DescriptionRole, "description"}, {FormatRole, "format"},
            {VisibilityRole, "visibility"}, {UrlRole, "url"}, {ArtRole, "art"}, {FandomRole, "fandomId"},
            {CreatedRole, "createdAt"}};
}

} // namespace katsodi
