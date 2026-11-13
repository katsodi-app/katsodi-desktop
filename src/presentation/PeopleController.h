#pragma once

#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace katsodi {

class ApiClient;

/**
 * Friends and profiles, through the native API and the website's own
 * actions and rules: friends, requests both ways, blocked people; asking by
 * @username with the website's answers (sent, already friends, already
 * asked, unavailable); a profile the caller may see (yourself or someone
 * you share an active room with). Late answers are dropped by generation.
 */
class PeopleController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList friends READ friends NOTIFY changed)
    Q_PROPERTY(QVariantList incoming READ incoming NOTIFY changed)
    Q_PROPERTY(QVariantList outgoing READ outgoing NOTIFY changed)
    Q_PROPERTY(QVariantList blocked READ blocked NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    /** The last friend request's outcome: "sent", "friends", "already", "unavailable" or "". */
    Q_PROPERTY(QString outcome READ outcome NOTIFY changed)
    /** The open profile: { id, username, displayName, bio, createdAt, level, xp, intoLevel, nextLevel,
     *  achievements, unlocked, fandoms, communityFandoms, self, relation }. */
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QString profileErrorKey READ profileErrorKey NOTIFY profileChanged)
    Q_PROPERTY(bool profileLoading READ profileLoading NOTIFY profileChanged)
public:
    explicit PeopleController(ApiClient *api, QObject *parent = nullptr);

    QVariantList friends() const { return m_friends; }
    QVariantList incoming() const { return m_incoming; }
    QVariantList outgoing() const { return m_outgoing; }
    QVariantList blocked() const { return m_blocked; }
    bool loading() const { return m_loading; }
    bool busy() const { return m_busy; }
    QString errorKey() const { return m_errorKey; }
    QString outcome() const { return m_outcome; }
    QVariantMap profile() const { return m_profile; }
    QString profileErrorKey() const { return m_profileError; }
    bool profileLoading() const { return m_profileLoading; }

    Q_INVOKABLE void reload();
    /** False (nothing sent) when the name is not a valid @username. */
    Q_INVOKABLE bool ask(const QString &username);
    Q_INVOKABLE void answer(const QString &requestId, bool accept);
    Q_INVOKABLE void withdraw(const QString &requestId);
    /** "remove", "block" or "unblock". */
    Q_INVOKABLE void change(const QString &userId, const QString &operation);
    Q_INVOKABLE void openProfile(const QString &username);

    /** Applies a friends overview (public for tests). */
    void applyOverview(const QJsonObject &body);
    /** Applies a profile answer (public for tests). */
    void applyProfile(const QJsonObject &body);
    /** "@Anna_1" → "anna_1"; "" when not a valid username (website rule). */
    static QString normalizeUsername(const QString &input);

signals:
    void changed();
    void profileChanged();

private:
    void act(const QByteArray &verb, const QString &path, const QJsonObject &body);
    QString relationOf(const QString &userId) const;

    ApiClient *m_api;
    QVariantList m_friends, m_incoming, m_outgoing, m_blocked;
    QVariantMap m_profile;
    QString m_errorKey, m_outcome, m_profileError, m_openUsername;
    bool m_loading = false, m_busy = false, m_profileLoading = false;
    quint64 m_generation = 0, m_profileGeneration = 0;
};

} // namespace katsodi
