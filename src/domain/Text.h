#pragma once
#include <QString>
#include <QStringList>

namespace katsodi {

/**
 * Search text as the website builds it (src/lib/search.ts): NFKD, combining
 * marks removed, lower case, ё → е, whitespace collapsed.
 */
QString searchText(const QString &value);

/** Every query word appears somewhere in the joined values. */
bool matchesSearch(const QString &query, const QStringList &values);

/** Canonical UUID text (any case). */
bool isUuid(const QString &value);

/** The website's chat limit: 1–2000 characters after trimming. */
bool isValidMessage(const QString &text);

/**
 * The website's structural source check (inspectSource in
 * src/lib/providers.ts, without provider recognition): https only, no
 * credentials, default port, a dotted host name that is neither an IP
 * literal nor a reserved name. The server checks again for shared rooms.
 */
bool isAcceptableSource(const QString &url);

/** Up to two initials for an artwork fallback, as on the website. */
QString initials(const QString &name);

/** The 11-character YouTube id of a watch/shorts/live/embed/youtu.be link, as on the website. */
QString youtubeVideoId(const QString &url);

/**
 * Providers with an official player API the room can synchronize:
 * "youtube", "vimeo", "soundcloud" (a single track) or "" (independent
 * playback). Mirrors syncSource() in src/lib/playback.ts.
 */
QString syncSource(const QString &url);

/** Replaces {key} placeholders. Unknown keys stay as written. */
QString formatText(QString text, const QString &key, const QString &value);

} // namespace katsodi
