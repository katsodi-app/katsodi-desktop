#pragma once
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace katsodi {

/** A failed request: a domain code from the server's error envelope, or a
 *  transport problem ("network", "invalid_response"). */
struct ApiError {
    QString code;
    QString messageKey; // id in the `native` strings
    int status = 0;
    bool retryable = false;
};

/** Supplies a current access token (refreshing first when needed), or an
 *  empty string for a guest request. */
class TokenSource {
public:
    virtual ~TokenSource() = default;
    virtual void withToken(QObject *context, std::function<void(const QString &token)> done) = 0;
};

/**
 * HTTPS JSON client for /api/native/v1 on one fixed origin.
 *
 * - The bearer token goes only to that origin; redirects are not followed
 *   at all, so it can never be forwarded elsewhere.
 * - Every request has a timeout and a response size limit, and runs
 *   asynchronously on the GUI thread's event loop (no nested loops).
 * - Callbacks are tied to a context object and never run after it is gone.
 * - POST bodies are never resent automatically.
 */
class ApiClient : public QObject {
    Q_OBJECT
public:
    using Done = std::function<void(const QJsonObject &body, const ApiError *error)>;

    ApiClient(const QUrl &origin, QObject *parent = nullptr);

    QUrl origin() const { return m_origin; }
    void setTokenSource(TokenSource *source) { m_tokens = source; }

    void get(const QString &path, QObject *context, Done done);
    void post(const QString &path, const QJsonObject &body, QObject *context, Done done);
    void del(const QString &path, QObject *context, Done done);

    /** Unauthenticated GET for public bootstrap data. */
    void getPublic(const QString &path, QObject *context, Done done);

    static constexpr qint64 maxResponseBytes = 4 * 1024 * 1024;
    static constexpr int timeoutMs = 15000;

signals:
    /** Whether the last request reached the server at all. */
    void reachabilityChanged(bool reachable);

private:
    void send(const QByteArray &verb, const QString &path, const QByteArray &body,
              const QString &token, QObject *context, Done done);
    void finish(QNetworkReply *reply, QPointer<QObject> context, const Done &done);

    QUrl m_origin;
    QNetworkAccessManager *m_network;
    TokenSource *m_tokens = nullptr;
    bool m_reachable = true;
};

} // namespace katsodi
