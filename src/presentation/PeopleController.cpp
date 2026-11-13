#include "presentation/PeopleController.h"

#include "domain/Text.h"
#include "infrastructure/api/ApiClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

#include <string_view>

namespace katsodi {
namespace {

QVariantMap person(const QJsonObject &json, const char *idField)
{
    const QString id = json.value(QLatin1StringView(idField)).toString().toLower();
    const QString username = json.value(QLatin1StringView("username")).toString().left(24);
    const QString name = json.value(QLatin1StringView("display_name")).toString().left(120);
    if (!isUuid(id) || username.isEmpty())
        return {};
    QVariantMap out{{QStringLiteral("userId"), id},
                    {QStringLiteral("username"), username},
                    {QStringLiteral("name"), name.isEmpty() ? username : name},
                    {QStringLiteral("initials"), initials(name.isEmpty() ? username : name)}};
    const QString since = json.value(QLatin1StringView("since")).toString();
    if (!since.isEmpty())
        out.insert(QStringLiteral("since"), since.left(10));
    if (std::string_view(idField) == "user_id") {
        const QString request = json.value(QLatin1StringView("id")).toString().toLower();
        if (!isUuid(request))
            return {};
        out.insert(QStringLiteral("requestId"), request);
    }
    return out;
}

QVariantList people(const QJsonArray &array, const char *idField)
{
    QVariantList out;
    for (const QJsonValue &item : array) {
        const QVariantMap entry = person(item.toObject(), idField);
        if (!entry.isEmpty())
            out.append(entry);
        if (out.size() >= 500)
            break;
    }
    return out;
}

} // namespace

PeopleController::PeopleController(ApiClient *api, QObject *parent) : QObject(parent), m_api(api) {}

QString PeopleController::normalizeUsername(const QString &input)
{
    QString name = input.trimmed();
    if (name.startsWith(u'@'))
        name.remove(0, 1);
    name = name.toLower();
    static const QRegularExpression valid(QStringLiteral("^[a-z][a-z0-9_]{2,23}$"));
    return valid.match(name).hasMatch() ? name : QString();
}

void PeopleController::reload()
{
    const quint64 generation = ++m_generation;
    m_loading = true;
    emit changed();
    m_api->get(QStringLiteral("/api/native/v1/friends"), this,
               [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation)
            return;
        m_loading = false;
        if (error) {
            m_errorKey = error->code == QLatin1StringView("unavailable") ? QStringLiteral("friendsUnavailable")
                                                                          : error->messageKey;
            emit changed();
            return;
        }
        m_errorKey.clear();
        applyOverview(body);
    });
}

void PeopleController::applyOverview(const QJsonObject &body)
{
    m_friends = people(body.value(QLatin1StringView("friends")).toArray(), "id");
    m_incoming = people(body.value(QLatin1StringView("incoming")).toArray(), "user_id");
    m_outgoing = people(body.value(QLatin1StringView("outgoing")).toArray(), "user_id");
    m_blocked = people(body.value(QLatin1StringView("blocked")).toArray(), "id");
    if (!m_profile.isEmpty()) {
        m_profile.insert(QStringLiteral("relation"), relationOf(m_profile.value(QStringLiteral("id")).toString()));
        emit profileChanged();
    }
    emit changed();
}

QString PeopleController::relationOf(const QString &userId) const
{
    const auto has = [&userId](const QVariantList &list) {
        for (const QVariant &item : list)
            if (item.toMap().value(QStringLiteral("userId")).toString() == userId)
                return true;
        return false;
    };
    if (m_profile.value(QStringLiteral("self")).toBool())
        return QStringLiteral("self");
    if (has(m_friends))
        return QStringLiteral("friend");
    if (has(m_incoming))
        return QStringLiteral("incoming");
    if (has(m_outgoing))
        return QStringLiteral("outgoing");
    if (has(m_blocked))
        return QStringLiteral("blocked");
    return QStringLiteral("none");
}

void PeopleController::act(const QByteArray &verb, const QString &path, const QJsonObject &body)
{
    if (m_busy)
        return;
    m_busy = true;
    m_errorKey.clear();
    emit changed();
    auto done = [this](const QJsonObject &answer, const ApiError *error) {
        m_busy = false;
        if (error) {
            m_errorKey = error->messageKey;
        } else if (answer.contains(QLatin1StringView("outcome"))) {
            static const QStringList outcomes{"sent", "friends", "already", "unavailable"};
            const QString outcome = answer.value(QLatin1StringView("outcome")).toString();
            m_outcome = outcomes.contains(outcome) ? outcome : QStringLiteral("unavailable");
        }
        emit changed();
        reload();
    };
    if (verb == "DELETE")
        m_api->del(path, this, done);
    else
        m_api->post(path, body, this, done);
}

bool PeopleController::ask(const QString &username)
{
    const QString name = normalizeUsername(username);
    if (name.isEmpty())
        return false;
    m_outcome.clear();
    act("POST", QStringLiteral("/api/native/v1/friends/requests"), QJsonObject{{QStringLiteral("username"), name}});
    return true;
}

void PeopleController::answer(const QString &requestId, bool accept)
{
    if (isUuid(requestId))
        act("POST", QStringLiteral("/api/native/v1/friends/requests/%1").arg(requestId),
            QJsonObject{{QStringLiteral("accept"), accept}});
}

void PeopleController::withdraw(const QString &requestId)
{
    if (isUuid(requestId))
        act("DELETE", QStringLiteral("/api/native/v1/friends/requests/%1").arg(requestId), {});
}

void PeopleController::change(const QString &userId, const QString &operation)
{
    static const QStringList operations{"remove", "block", "unblock"};
    if (isUuid(userId) && operations.contains(operation))
        act("POST", QStringLiteral("/api/native/v1/people/%1").arg(userId),
            QJsonObject{{QStringLiteral("operation"), operation}});
}

void PeopleController::openProfile(const QString &username)
{
    const QString name = normalizeUsername(username);
    const quint64 generation = ++m_profileGeneration;
    m_openUsername = name;
    m_profile.clear();
    m_profileError = name.isEmpty() ? QStringLiteral("errorNotFound") : QString();
    m_profileLoading = !name.isEmpty();
    emit profileChanged();
    if (name.isEmpty())
        return;
    m_api->get(QStringLiteral("/api/native/v1/profiles/%1").arg(name), this,
               [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_profileGeneration)
            return;
        m_profileLoading = false;
        if (error) {
            m_profileError = error->messageKey;
            emit profileChanged();
            return;
        }
        applyProfile(body);
    });
    reload(); // the relation (friend, asked, blocked) comes from the overview
}

void PeopleController::applyProfile(const QJsonObject &body)
{
    const QString id = body.value(QLatin1StringView("id")).toString().toLower();
    const QString username = body.value(QLatin1StringView("username")).toString().left(24);
    if (!isUuid(id) || username.isEmpty()) {
        m_profileError = QStringLiteral("errorServer");
        emit profileChanged();
        return;
    }
    const QString name = body.value(QLatin1StringView("display_name")).toString().left(120);
    QVariantMap profile{{QStringLiteral("id"), id},
                        {QStringLiteral("username"), username},
                        {QStringLiteral("displayName"), name.isEmpty() ? username : name},
                        {QStringLiteral("initials"), initials(name.isEmpty() ? username : name)},
                        {QStringLiteral("bio"), body.value(QLatin1StringView("bio")).toString().left(1000)},
                        {QStringLiteral("createdAt"), body.value(QLatin1StringView("created_at")).toString().left(10)},
                        {QStringLiteral("self"), body.value(QLatin1StringView("self")).toBool()}};
    QStringList fandoms;
    for (const QJsonValue &f : body.value(QLatin1StringView("fandoms")).toArray())
        if (fandoms.size() < 100)
            fandoms.append(f.toString().left(40));
    profile.insert(QStringLiteral("fandoms"), fandoms);
    QVariantList community;
    for (const QJsonValue &f : body.value(QLatin1StringView("communityFandoms")).toArray())
        community.append(QVariantMap{{QStringLiteral("id"), f.toObject().value("id").toString().left(40)},
                                     {QStringLiteral("name"), f.toObject().value("name").toString().left(120)}});
    profile.insert(QStringLiteral("communityFandoms"), community);
    const QJsonObject progress = body.value(QLatin1StringView("progress")).toObject();
    if (!progress.isEmpty()) {
        profile.insert(QStringLiteral("level"), progress.value(QLatin1StringView("level")).toInt(1));
        profile.insert(QStringLiteral("xp"), progress.value(QLatin1StringView("xp")).toInt());
        profile.insert(QStringLiteral("intoLevel"), progress.value(QLatin1StringView("into_level")).toInt());
        profile.insert(QStringLiteral("nextLevel"), qMax(1, progress.value(QLatin1StringView("next_level")).toInt(1)));
        int unlocked = 0;
        const QJsonArray achievements = progress.value(QLatin1StringView("achievements")).toArray();
        for (const QJsonValue &a : achievements)
            unlocked += !a.toObject().value(QLatin1StringView("unlocked_at")).isNull();
        profile.insert(QStringLiteral("achievements"), int(achievements.size()));
        profile.insert(QStringLiteral("unlocked"), unlocked);
    }
    m_profile = profile;
    m_profile.insert(QStringLiteral("relation"), relationOf(id));
    m_profileError.clear();
    emit profileChanged();
}

} // namespace katsodi
