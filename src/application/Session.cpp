#include "application/Session.h"

#include "BuildConfig.h"
#include "domain/Text.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace katsodi {

Session::Session(ApiClient *api, CredentialStore credentials, QObject *parent)
    : QObject(parent), m_api(api), m_credentials(std::move(credentials)),
      m_auth(new QNetworkAccessManager(this))
{
    m_auth->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    m_auth->setTransferTimeout(ApiClient::timeoutMs);
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &Session::refresh);
    connect(m_api, &ApiClient::reachabilityChanged, this, [this](bool reachable) {
        if (m_server == QLatin1StringView("unconfigured"))
            return;
        m_server = reachable ? QStringLiteral("online") : QStringLiteral("offline");
        emit changed();
    });
}

bool Session::acceptableAuthUrl(const QUrl &url, bool development)
{
    if (!url.isValid() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment())
        return false;
    if (url.scheme() == QLatin1StringView("https"))
        return true;
    return development && url.scheme() == QLatin1StringView("http")
           && (url.host() == QLatin1StringView("127.0.0.1") || url.host() == QLatin1StringView("localhost"));
}

void Session::start()
{
    m_server = QStringLiteral("checking");
    emit changed();
    const quint64 generation = m_generation;
    m_api->getPublic(QStringLiteral("/api/native/v1/meta"), this,
                     [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation)
            return;
        if (error) {
            m_server = QStringLiteral("offline");
            setState(QStringLiteral("signedOut"));
            return;
        }
        if (body.value(QLatin1StringView("apiVersion")).toInt() != 1) {
            m_server = QStringLiteral("unconfigured");
            setState(QStringLiteral("signedOut"));
            return;
        }
        const QJsonObject supabase = body.value(QLatin1StringView("supabase")).toObject();
        const QUrl url(supabase.value(QLatin1StringView("url")).toString());
        const QString key = supabase.value(QLatin1StringView("publishableKey")).toString();
        if (!acceptableAuthUrl(url, build::development) || key.isEmpty() || key.size() > 400) {
            m_server = QStringLiteral("unconfigured");
            setState(QStringLiteral("signedOut"));
            return;
        }
        m_authUrl = url;
        m_publishableKey = key;
        m_server = QStringLiteral("online");
        if (signedIn()) {
            emit changed();
            return;
        }
        const auto stored = m_credentials.read();
        if (!stored) {
            setState(QStringLiteral("signedOut"));
            return;
        }
        m_refreshToken = *stored;
        refresh();
    });
}

void Session::setState(const QString &state, const QString &errorKey)
{
    m_state = state;
    m_errorKey = errorKey;
    emit changed();
}

void Session::authRequest(const QString &grant, const QJsonObject &body,
                          std::function<void(int, const QJsonObject &)> done)
{
    QUrl url = m_authUrl;
    url.setPath(QStringLiteral("/auth/v1/token"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("grant_type"), grant);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("apikey", m_publishableKey.toUtf8());
    QNetworkReply *reply = m_auth->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->read(256 * 1024);
        done(status, QJsonDocument::fromJson(data).object());
    });
}

void Session::signIn(const QString &email, const QString &password)
{
    if (m_state == QLatin1StringView("authenticating"))
        return;
    if (m_authUrl.isEmpty()) {
        setState(QStringLiteral("signedOut"), QStringLiteral("signInNetwork"));
        start();
        return;
    }
    if (email.trimmed().isEmpty() || email.size() > 320 || password.isEmpty() || password.size() > 128) {
        setState(QStringLiteral("signedOut"), QStringLiteral("signInFailed"));
        return;
    }
    setState(QStringLiteral("authenticating"));
    const quint64 generation = ++m_generation;
    authRequest(QStringLiteral("password"),
                QJsonObject{{QStringLiteral("email"), email.trimmed()}, {QStringLiteral("password"), password}},
                [this, generation](int status, const QJsonObject &reply) {
        if (generation != m_generation)
            return;
        if (status == 0) {
            setState(QStringLiteral("signedOut"), QStringLiteral("signInNetwork"));
            return;
        }
        if (status != 200) {
            const QString code = reply.value(QLatin1StringView("error_code")).toString();
            setState(QStringLiteral("signedOut"), code == QLatin1StringView("email_not_confirmed")
                                                      ? QStringLiteral("signInUnconfirmed")
                                                      : QStringLiteral("signInFailed"));
            return;
        }
        const QJsonObject user = reply.value(QLatin1StringView("user")).toObject();
        if (user.value(QLatin1StringView("email_confirmed_at")).toString().isEmpty()) {
            setState(QStringLiteral("signedOut"), QStringLiteral("signInUnconfirmed"));
            return;
        }
        acceptTokens(reply);
    });
}

void Session::acceptTokens(const QJsonObject &reply)
{
    const QString access = reply.value(QLatin1StringView("access_token")).toString();
    const QString refreshToken = reply.value(QLatin1StringView("refresh_token")).toString();
    const int expiresIn = reply.value(QLatin1StringView("expires_in")).toInt();
    const QJsonObject user = reply.value(QLatin1StringView("user")).toObject();
    const QString id = user.value(QLatin1StringView("id")).toString();
    if (access.isEmpty() || access.size() > 8192 || refreshToken.isEmpty() || refreshToken.size() > 1024
        || expiresIn <= 0 || !isUuid(id)) {
        clear();
        setState(QStringLiteral("signedOut"), QStringLiteral("errorServer"));
        return;
    }
    m_accessToken = access;
    m_refreshToken = refreshToken;
    m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    m_userId = id.toLower();
    m_email = user.value(QLatin1StringView("email")).toString().left(320);
    if (m_displayName.isEmpty())
        m_displayName = m_email;
    // Rotation: the new refresh token replaces the old one at once.
    if (m_credentials.available())
        m_credentials.write(refreshToken);
    m_refreshTimer.start(std::max(10, expiresIn - 60) * 1000);
    setState(QStringLiteral("signedIn"));
    loadProfile();
}

void Session::loadProfile()
{
    const quint64 generation = m_generation;
    m_api->get(QStringLiteral("/api/native/v1/me"), this,
               [this, generation](const QJsonObject &body, const ApiError *error) {
        if (generation != m_generation || error)
            return;
        const QJsonObject profile = body.value(QLatin1StringView("profile")).toObject();
        const QString name = profile.value(QLatin1StringView("displayName")).toString().left(120);
        if (!name.isEmpty()) {
            m_displayName = name;
            emit changed();
        }
    });
}

void Session::refresh()
{
    if (m_refreshing)
        return;
    if (m_refreshToken.isEmpty() || m_authUrl.isEmpty()) {
        finishRefresh(false);
        return;
    }
    m_refreshing = true;
    const quint64 generation = m_generation;
    authRequest(QStringLiteral("refresh_token"),
                QJsonObject{{QStringLiteral("refresh_token"), m_refreshToken}},
                [this, generation](int status, const QJsonObject &reply) {
        m_refreshing = false;
        if (generation != m_generation) {
            finishRefresh(false);
            return;
        }
        if (status == 200) {
            acceptTokens(reply);
            finishRefresh(signedIn());
            return;
        }
        if (status == 0) {
            // Offline: keep the session; requests fail and are retried later.
            if (m_state == QLatin1StringView("starting"))
                setState(QStringLiteral("signedOut"), QStringLiteral("signInNetwork"));
            m_refreshTimer.start(30000);
            finishRefresh(false);
            return;
        }
        // Revoked, rotated elsewhere or expired: the stored session is over.
        clear();
        setState(QStringLiteral("signedOut"), QStringLiteral("errorAuth"));
        finishRefresh(false);
    });
}

void Session::finishRefresh(bool ok)
{
    auto waiting = std::move(m_waiting);
    m_waiting.clear();
    for (auto &[context, done] : waiting)
        if (context)
            done(ok ? m_accessToken : QString());
}

void Session::withToken(QObject *context, std::function<void(const QString &)> done)
{
    if (!signedIn()) {
        done({});
        return;
    }
    if (QDateTime::currentDateTimeUtc().secsTo(m_expiresAt) > 30) {
        done(m_accessToken);
        return;
    }
    m_waiting.emplace_back(QPointer<QObject>(context), std::move(done));
    refresh();
}

void Session::signOut()
{
    // Best effort server-side logout; the local session ends regardless.
    if (!m_accessToken.isEmpty() && !m_authUrl.isEmpty()) {
        QUrl url = m_authUrl;
        url.setPath(QStringLiteral("/auth/v1/logout"));
        url.setQuery(QStringLiteral("scope=local"));
        QNetworkRequest request(url);
        request.setRawHeader("apikey", m_publishableKey.toUtf8());
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());
        QNetworkReply *reply = m_auth->post(request, QByteArray());
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    clear();
    setState(QStringLiteral("signedOut"));
}

void Session::clear()
{
    ++m_generation; // late replies of the old session are ignored
    m_refreshTimer.stop();
    m_accessToken.fill(u'\0');
    m_accessToken.clear();
    m_refreshToken.fill(u'\0');
    m_refreshToken.clear();
    m_expiresAt = {};
    m_userId.clear();
    m_displayName.clear();
    m_email.clear();
    m_credentials.remove();
    finishRefresh(false);
}

} // namespace katsodi
