#include "presentation/FandomModel.h"

#include "domain/Text.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace katsodi {

FandomModel::FandomModel(const QUrl &artworkBase, QObject *parent)
    : QAbstractListModel(parent), m_artworkBase(artworkBase)
{
    QFile file(QStringLiteral(":/katsodi/generated/fandoms.json"));
    if (file.open(QIODevice::ReadOnly))
        load(file.readAll());
}

bool FandomModel::load(const QByteArray &json)
{
    static const QRegularExpression idPattern(QStringLiteral("^[a-z]+-\\d{1,3}$"));
    static const QRegularExpression artPattern(QStringLiteral("^[a-z]+-\\d+(-poster(-480)?)?\\.webp$"));
    static const QStringList categories{"cinema", "anime", "games", "music", "books", "sport"};
    std::vector<Fandom> loaded;
    const QJsonArray items = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &item : items) {
        const QJsonObject object = item.toObject();
        Fandom fandom;
        fandom.id = object.value(QLatin1StringView("id")).toString();
        fandom.name = object.value(QLatin1StringView("name")).toString();
        fandom.category = object.value(QLatin1StringView("category")).toString();
        if (!idPattern.match(fandom.id).hasMatch() || fandom.name.isEmpty() || fandom.name.size() > 120
            || !categories.contains(fandom.category))
            continue;
        const QJsonObject art = object.value(QLatin1StringView("art")).toObject();
        const QString file = art.value(QLatin1StringView("file")).toString();
        if (artPattern.match(file).hasMatch()) {
            fandom.art = file;
            fandom.width = art.value(QLatin1StringView("width")).toInt();
            fandom.height = art.value(QLatin1StringView("height")).toInt();
        }
        fandom.initials = initials(fandom.name);
        loaded.push_back(std::move(fandom));
    }
    beginResetModel();
    m_all = std::move(loaded);
    m_visible.clear();
    for (int i = 0; i < static_cast<int>(m_all.size()); ++i)
        m_visible.push_back(i);
    endResetModel();
    return !m_all.empty();
}

void FandomModel::refilter()
{
    std::vector<int> visible;
    for (int i = 0; i < static_cast<int>(m_all.size()); ++i) {
        const Fandom &fandom = m_all[static_cast<size_t>(i)];
        if (!m_category.isEmpty() && fandom.category != m_category)
            continue;
        if (!m_query.isEmpty() && !matchesSearch(m_query, {fandom.name}))
            continue;
        visible.push_back(i);
    }
    if (visible == m_visible)
        return;
    beginResetModel();
    m_visible = std::move(visible);
    endResetModel();
}

void FandomModel::setCategory(const QString &category)
{
    if (category == m_category)
        return;
    m_category = category;
    refilter();
    emit filterChanged();
}

void FandomModel::setQuery(const QString &query)
{
    const QString bounded = query.left(200);
    if (bounded == m_query)
        return;
    m_query = bounded;
    refilter();
    emit filterChanged();
}

QString FandomModel::nameOf(const QString &id) const
{
    for (const Fandom &fandom : m_all)
        if (fandom.id == id)
            return fandom.name;
    return {};
}

QUrl FandomModel::artOf(const QString &id) const
{
    for (const Fandom &fandom : m_all)
        if (fandom.id == id)
            return fandom.art.isEmpty() ? QUrl() : m_artworkBase.resolved(QUrl(fandom.art));
    return {};
}

int FandomModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_visible.size());
}

QVariant FandomModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_visible.size()))
        return {};
    const Fandom &fandom = m_all[static_cast<size_t>(m_visible[static_cast<size_t>(index.row())])];
    switch (role) {
    case IdRole: return fandom.id;
    case NameRole: return fandom.name;
    case CategoryRole: return fandom.category;
    case ArtRole: return fandom.art.isEmpty() ? QUrl() : m_artworkBase.resolved(QUrl(fandom.art));
    case ArtWidthRole: return fandom.width;
    case ArtHeightRole: return fandom.height;
    case InitialsRole: return fandom.initials;
    default: return {};
    }
}

QHash<int, QByteArray> FandomModel::roleNames() const
{
    return {{IdRole, "fandomId"}, {NameRole, "name"}, {CategoryRole, "category"}, {ArtRole, "art"},
            {ArtWidthRole, "artWidth"}, {ArtHeightRole, "artHeight"}, {InitialsRole, "initials"}};
}

} // namespace katsodi
