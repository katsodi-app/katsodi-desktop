#include "domain/Text.h"

#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace katsodi {

QString searchText(const QString &value)
{
    const QString decomposed = value.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(decomposed.size());
    bool space = false;
    for (const QChar ch : decomposed) {
        const auto category = ch.category();
        if (category == QChar::Mark_NonSpacing || category == QChar::Mark_SpacingCombining
            || category == QChar::Mark_Enclosing)
            continue;
        if (ch.isSpace()) {
            space = !out.isEmpty();
            continue;
        }
        if (space) {
            out.append(u' ');
            space = false;
        }
        QChar lower = ch.toLower();
        if (lower == QChar(0x0451)) // ё
            lower = QChar(0x0435);  // е
        out.append(lower);
    }
    return out;
}

bool matchesSearch(const QString &query, const QStringList &values)
{
    const QString text = searchText(values.join(u' '));
    const QStringList tokens = searchText(query).split(u' ', Qt::SkipEmptyParts);
    for (const QString &token : tokens)
        if (!text.contains(token))
            return false;
    return true;
}

bool isUuid(const QString &value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return pattern.match(value).hasMatch();
}

bool isValidMessage(const QString &text)
{
    const QString trimmed = text.trimmed();
    return !trimmed.isEmpty() && trimmed.size() <= 2000;
}

bool isAcceptableSource(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty() || input.size() > 2048)
        return false;
    for (const QChar ch : trimmed)
        if (ch.unicode() <= 0x20 || ch.unicode() == 0x7f)
            return false;
    const QUrl url(trimmed, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QLatin1StringView("https") || !url.userName().isEmpty()
        || !url.password().isEmpty() || (url.port() != -1 && url.port() != 443))
        return false;
    const QString host = url.host().toLower();
    static const QRegularExpression ipv4(QStringLiteral(R"(^[\d.]+$)"));
    static const QRegularExpression reserved(QStringLiteral(R"(\.(localhost|local|internal|test|invalid|example)$)"));
    return host.contains(u'.') && !ipv4.match(host).hasMatch() && !host.contains(u':') && !host.startsWith(u'[')
           && !reserved.match(host).hasMatch() && !host.endsWith(u'.');
}

QString youtubeVideoId(const QString &input)
{
    if (!isAcceptableSource(input))
        return {};
    const QUrl url(input.trimmed(), QUrl::StrictMode);
    const QString host = url.host().toLower();
    static const QStringList hosts{QStringLiteral("youtube.com"), QStringLiteral("www.youtube.com"),
                                   QStringLiteral("m.youtube.com"), QStringLiteral("youtu.be")};
    if (!hosts.contains(host))
        return {};
    QString id;
    const QString path = url.path();
    if (host == QLatin1StringView("youtu.be")) {
        id = path.mid(1);
    } else if (path == QLatin1StringView("/watch")) {
        id = QUrlQuery(url).queryItemValue(QStringLiteral("v"));
    } else {
        static const QRegularExpression embed(QStringLiteral(R"(^/(?:embed|shorts|live)/([^/]+)$)"));
        const auto match = embed.match(path);
        if (match.hasMatch())
            id = match.captured(1);
    }
    static const QRegularExpression valid(QStringLiteral(R"(^[A-Za-z0-9_-]{11}$)"));
    return valid.match(id).hasMatch() ? id : QString();
}

QString syncSource(const QString &input)
{
    if (!youtubeVideoId(input).isEmpty())
        return QStringLiteral("youtube");
    if (!isAcceptableSource(input))
        return {};
    const QUrl url(input.trimmed(), QUrl::StrictMode);
    const QString host = url.host().toLower();
    if (host == QLatin1StringView("vimeo.com") || host == QLatin1StringView("www.vimeo.com"))
        return QStringLiteral("vimeo");
    static const QRegularExpression track(QStringLiteral(R"(^/[^/]+/[^/]+/?$)"));
    if ((host == QLatin1StringView("soundcloud.com") || host == QLatin1StringView("www.soundcloud.com")
         || host == QLatin1StringView("m.soundcloud.com"))
        && track.match(url.path()).hasMatch())
        return QStringLiteral("soundcloud");
    return {};
}

QString initials(const QString &name)
{
    QString out;
    for (const QString &word : name.split(u' ', Qt::SkipEmptyParts)) {
        for (const QChar ch : word)
            if (ch.isLetterOrNumber()) {
                out.append(ch.toUpper());
                break;
            }
        if (out.size() == 2)
            break;
    }
    return out.isEmpty() ? QStringLiteral("K") : out;
}

QString formatText(QString text, const QString &key, const QString &value)
{
    return text.replace(u'{' + key + u'}', value);
}

} // namespace katsodi
