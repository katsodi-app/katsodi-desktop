#include "infrastructure/api/ApiClient.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace katsodi {
namespace {

bool validPath(const QString &path)
{
    // Fixed API prefix, no scheme/host, no traversal, no control characters.
    if (!path.startsWith(QLatin1StringView("/api/native/v1/")) || path.size() > 512)
        return false;
    if (path.contains(QLatin1StringView("..")) || path.contains(u'\\') || path.contains(u'#'))
        return false;
    for (const QChar ch : path)
        if (ch.unicode() < 0x21 || ch.unicode() == 0x7f)
            return false;
    return true;
}

ApiError transportError(const QString &code)
{
    return ApiError{code, QStringLiteral("errorNetwork"), 0, true};
}

} // namespace

ApiClient::ApiClient(const QUrl &origin, QObject *parent)
    : QObject(parent), m_origin(origin), m_network(new QNetworkAccessManager(this))
{
    m_network->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    m_network->setTransferTimeout(timeoutMs);
}

void ApiClient::get(const QString &path, QObject *context, Done done)
{
    if (!m_tokens)
        return send("GET", path, {}, {}, context, std::move(done));
    m_tokens->withToken(context, [this, path, context, done = std::move(done)](const QString &token) {
        send("GET", path, {}, token, context, done);
    });
}

void ApiClient::post(const QString &path, const QJsonObject &body, QObject *context, Done done)
{
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (!m_tokens)
        return send("POST", path, payload, {}, context, std::move(done));
    m_tokens->withToken(context, [this, path, payload, context, done = std::move(done)](const QString &token) {
        send("POST", path, payload, token, context, done);
    });
}

void ApiClient::del(const QString &path, QObject *context, Done done)
{
    if (!m_tokens)
        return send("DELETE", path, {}, {}, context, std::move(done));
    m_tokens->withToken(context, [this, path, context, done = std::move(done)](const QString &token) {
        send("DELETE", path, {}, token, context, done);
    });
}

void ApiClient::getPublic(const QString &path, QObject *context, Done done)
{
    send("GET", path, {}, {}, context, std::move(done));
}

void ApiClient::send(const QByteArray &verb, const QString &path, const QByteArray &body,
                     const QString &token, QObject *context, Done done)
{
    QPointer<QObject> guard(context);
    if (!validPath(path)) {
        const ApiError error{QStringLiteral("invalid_request"), QStringLiteral("errorInvalid"), 0, false};
        if (guard)
            done({}, &error);
        return;
    }
    QUrl url = m_origin;
    const qsizetype query = path.indexOf(u'?');
    url.setPath(query < 0 ? path : path.left(query));
    if (query >= 0)
        url.setQuery(path.mid(query + 1));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("KatsodiDesktop/0.1"));
    request.setRawHeader("Accept", "application/json");
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toLatin1());
    QNetworkReply *reply = nullptr;
    if (verb == "POST") {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_network->post(request, body);
    } else if (verb == "DELETE") {
        reply = m_network->deleteResource(request);
    } else {
        reply = m_network->get(request);
    }
    // Abort oversized bodies early instead of buffering them.
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > maxResponseBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, guard, done = std::move(done)]() {
        finish(reply, guard, done);
    });
}

void ApiClient::finish(QNetworkReply *reply, QPointer<QObject> context, const Done &done)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool reached = status > 0;
    if (reached != m_reachable) {
        m_reachable = reached;
        emit reachabilityChanged(reached);
    }
    if (!context)
        return; // The screen that asked is gone.
    if (!reached) {
        const ApiError error = transportError(QStringLiteral("network"));
        done({}, &error);
        return;
    }
    const QByteArray data = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument document = data.size() <= maxResponseBytes
                                       ? QJsonDocument::fromJson(data, &parseError)
                                       : QJsonDocument();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        ApiError error = transportError(QStringLiteral("invalid_response"));
        error.status = status;
        error.messageKey = QStringLiteral("errorServer");
        done({}, &error);
        return;
    }
    const QJsonObject object = document.object();
    if (status >= 200 && status < 300) {
        done(object, nullptr);
        return;
    }
    const QJsonObject envelope = object.value(QLatin1StringView("error")).toObject();
    ApiError error;
    error.status = status;
    error.code = envelope.value(QLatin1StringView("code")).toString(QStringLiteral("server_error")).left(40);
    const QString key = envelope.value(QLatin1StringView("messageKey")).toString();
    // Server keys are "native.<id>"; only that namespace is accepted.
    error.messageKey = key.startsWith(QLatin1StringView("native.")) && key.size() < 60
                           ? key.mid(7)
                           : QStringLiteral("errorServer");
    error.retryable = envelope.value(QLatin1StringView("retryable")).toBool(false);
    done({}, &error);
}

} // namespace katsodi
