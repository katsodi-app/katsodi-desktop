#pragma once
#include "infrastructure/api/ApiClient.h"
#include "infrastructure/credentials/CredentialStore.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <functional>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

namespace katsodi {

/**
 * The account session of the native client.
 *
 *   starting -> signedOut | signedIn            (stored refresh token)
 *   signedOut -> authenticating -> signedIn | signedOut (+ error)
 *   signedIn -> refreshing (internal) -> signedIn | signedOut
 *   any -> signOut() -> signedOut                (work cancelled)
 *
 * Sign-in talks to Supabase Auth directly as a public client, with the
 * project URL and publishable key published by /api/native/v1/meta. The
 * access token lives only in memory; the refresh token goes to the OS
 * credential store. One refresh serves all waiting requests.
 */
class Session : public QObject, public TokenSource {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(QString userId READ userId NOTIFY changed)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    Q_PROPERTY(QString email READ email NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    Q_PROPERTY(bool persistent READ persistent CONSTANT)
    Q_PROPERTY(QString server READ server NOTIFY changed)
public:
    Session(ApiClient *api, CredentialStore credentials, QObject *parent = nullptr);

    QString state() const { return m_state; }
    bool signedIn() const { return m_state == QLatin1StringView("signedIn"); }
    QString userId() const { return m_userId; }
    QString displayName() const { return m_displayName; }
    QString email() const { return m_email; }
    QString errorKey() const { return m_errorKey; }
    bool persistent() const { return m_credentials.available(); }
    /** "checking", "online", "offline" or "unconfigured". */
    QString server() const { return m_server; }

    /** Reads the public bootstrap and restores a stored session. */
    void start();
    Q_INVOKABLE void retryServer() { start(); }
    Q_INVOKABLE void signIn(const QString &email, const QString &password);
    Q_INVOKABLE void signOut();

    void withToken(QObject *context, std::function<void(const QString &token)> done) override;

    /** Validates the Supabase project URL from the server's bootstrap. */
    static bool acceptableAuthUrl(const QUrl &url, bool development);

signals:
    void changed();

private:
    void setState(const QString &state, const QString &errorKey = {});
    void authRequest(const QString &grant, const QJsonObject &body,
                     std::function<void(int status, const QJsonObject &reply)> done);
    void acceptTokens(const QJsonObject &reply);
    void refresh();
    void finishRefresh(bool ok);
    void loadProfile();
    void clear();

    ApiClient *m_api;
    CredentialStore m_credentials;
    QNetworkAccessManager *m_auth;
    QUrl m_authUrl;
    QString m_publishableKey;
    QString m_state = QStringLiteral("starting");
    QString m_server = QStringLiteral("checking");
    QString m_errorKey;
    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_expiresAt;
    QString m_userId, m_displayName, m_email;
    bool m_refreshing = false;
    std::vector<std::pair<QPointer<QObject>, std::function<void(const QString &)>>> m_waiting;
    QTimer m_refreshTimer;
    quint64 m_generation = 0;
};

} // namespace katsodi
