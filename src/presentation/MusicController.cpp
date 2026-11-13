#include "presentation/MusicController.h"

#include "domain/Text.h"
#include "infrastructure/api/ApiClient.h"
#include "presentation/RoomController.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantMap>

namespace katsodi {
namespace {

QString joined(const QJsonValue &artists)
{
    QStringList names;
    for (const QJsonValue &name : artists.toArray())
        if (names.size() < 6)
            names.append(name.toString().left(120));
    return names.join(QStringLiteral(", "));
}

// Only https artwork from the providers is shown.
QString cover(const QJsonValue &value)
{
    const QUrl url(value.toString());
    return url.isValid() && url.scheme() == QLatin1StringView("https") && url.toString().size() <= 2048
               ? url.toString()
               : QString();
}

QVariantMap track(const QJsonObject &json, const QString &url, const QString &kind)
{
    return {{QStringLiteral("rank"), json.value(QLatin1StringView("rank")).toInt()},
            {QStringLiteral("title"), json.value(QLatin1StringView("title")).toString().left(200)},
            {QStringLiteral("artists"), joined(json.value(QLatin1StringView("artists")))},
            {QStringLiteral("url"), url},
            {QStringLiteral("cover"), cover(json.value(json.contains(QLatin1StringView("thumb")) ? "thumb" : "cover"))},
            {QStringLiteral("plays"), json.value(json.contains(QLatin1StringView("streams")) ? "streams" : "plays").toDouble()},
            {QStringLiteral("duration"), json.value(QLatin1StringView("duration")).toInt()},
            {QStringLiteral("kind"), kind}};
}

} // namespace

MusicController::MusicController(ApiClient *api, RoomController *room, QObject *parent)
    : QObject(parent), m_api(api), m_room(room)
{
    connect(m_room, &RoomController::stateChanged, this, &MusicController::changed);
}

bool MusicController::roomTakesTracks() const
{
    return !m_room->roomId().isEmpty() && m_room->joined() && !m_room->closed()
           && m_room->format() == QLatin1StringView("music");
}

void MusicController::reload()
{
    if (m_loading)
        return;
    m_loading = true;
    emit changed();
    m_api->get(QStringLiteral("/api/native/v1/music"), this, [this](const QJsonObject &body, const ApiError *error) {
        m_loading = false;
        if (error) {
            m_errorKey = error->messageKey;
            emit changed();
            return;
        }
        m_errorKey.clear();
        applyLibrary(body);
    });
}

void MusicController::applyLibrary(const QJsonObject &body)
{
    m_shelfList.clear();
    m_shelfTracks.clear();
    const auto add = [this](const QString &id, const QString &provider, const QString &checked, bool stale,
                            const QVariantList &tracks) {
        if (tracks.isEmpty())
            return;
        m_shelfList.append(QVariantMap{{QStringLiteral("id"), id},
                                       {QStringLiteral("provider"), provider},
                                       {QStringLiteral("checked"), checked.left(10)},
                                       {QStringLiteral("stale"), stale}});
        m_shelfTracks.insert(id, tracks);
    };

    // The all-time chart: Spotify tracks by id.
    const QJsonObject chart = body.value(QLatin1StringView("chart")).toObject();
    QVariantList allTime;
    for (const QJsonValue &item : chart.value(QLatin1StringView("tracks")).toArray()) {
        const QJsonObject json = item.toObject();
        const QString id = json.value(QLatin1StringView("spotifyId")).toString();
        static const QRegularExpression spotifyId(QStringLiteral("^[A-Za-z0-9]{22}$"));
        if (!spotifyId.match(id).hasMatch())
            continue;
        allTime.append(track(json, QStringLiteral("https://open.spotify.com/track/") + id, QStringLiteral("spotify")));
    }
    add(QStringLiteral("all-time"), QStringLiteral("all-time"), chart.value(QLatin1StringView("checked")).toString(),
        false, allTime);

    // The providers' current charts.
    for (const QJsonValue &item : body.value(QLatin1StringView("charts")).toArray()) {
        const QJsonObject json = item.toObject();
        const QString provider = json.value(QLatin1StringView("provider")).toString().left(30);
        QVariantList tracks;
        for (const QJsonValue &t : json.value(QLatin1StringView("tracks")).toArray()) {
            const QJsonObject entry = t.toObject();
            const QString url = entry.value(QLatin1StringView("url")).toString();
            if (isAcceptableSource(url))
                tracks.append(track(entry, url, provider));
        }
        add(QStringLiteral("chart-") + provider, provider, json.value(QLatin1StringView("checked")).toString(),
            json.value(QLatin1StringView("status")).toString() == QLatin1StringView("stale"), tracks);
    }

    // Popular and trending collections.
    const QJsonObject collections = body.value(QLatin1StringView("collections")).toObject();
    for (const char *name : {"popular", "trending"}) {
        const QJsonObject collection = collections.value(QLatin1StringView(name)).toObject();
        QVariantList tracks;
        for (const QJsonValue &t : collection.value(QLatin1StringView("tracks")).toArray()) {
            const QJsonObject entry = t.toObject();
            const QString url = entry.value(QLatin1StringView("url")).toString();
            if (isAcceptableSource(url))
                tracks.append(track(entry, url, QStringLiteral("soundcloud")));
        }
        add(QString::fromLatin1(name), QString::fromLatin1(name), collection.value(QLatin1StringView("checked")).toString(),
            false, tracks);
    }
    m_loaded = true;
    if (!m_shelfTracks.contains(m_shelf) && !m_shelfList.isEmpty())
        m_shelf = m_shelfList.first().toMap().value(QStringLiteral("id")).toString();
    refilter();
}

void MusicController::setShelf(const QString &id)
{
    if (id == m_shelf || !m_shelfTracks.contains(id))
        return;
    m_shelf = id;
    refilter();
}

void MusicController::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query.left(200);
    refilter();
}

void MusicController::refilter()
{
    m_visible.clear();
    for (const QVariant &item : m_shelfTracks.value(m_shelf)) {
        const QVariantMap t = item.toMap();
        if (matchesSearch(m_query, {t.value(QStringLiteral("title")).toString(), t.value(QStringLiteral("artists")).toString()}))
            m_visible.append(t);
    }
    emit changed();
}

void MusicController::listen(const QString &url, const QString &title)
{
    if (m_busy || url.size() > 2048)
        return;
    m_busy = true;
    m_errorKey.clear();
    emit changed();
    QJsonObject body{{QStringLiteral("url"), url}, {QStringLiteral("title"), title.left(80)}};
    if (roomTakesTracks())
        body.insert(QStringLiteral("room"), m_room->roomId());
    m_api->post(QStringLiteral("/api/native/v1/music/listen"), body, this,
                [this](const QJsonObject &answer, const ApiError *error) {
        m_busy = false;
        m_errorKey = error ? error->messageKey : QString();
        emit changed();
        const QString roomId = answer.value(QLatin1StringView("roomId")).toString();
        if (!error && isUuid(roomId))
            emit openRoom(roomId);
    });
}

} // namespace katsodi
