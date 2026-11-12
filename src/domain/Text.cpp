#include "domain/Text.h"

#include <QRegularExpression>

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
