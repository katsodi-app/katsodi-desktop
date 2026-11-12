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

/** Up to two initials for an artwork fallback, as on the website. */
QString initials(const QString &name);

/** Replaces {key} placeholders. Unknown keys stay as written. */
QString formatText(QString text, const QString &key, const QString &value);

} // namespace katsodi
