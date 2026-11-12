#pragma once
#include <QString>
#include <optional>

namespace katsodi {

/**
 * Keeps the Supabase refresh token in the operating system's credential
 * store (Windows Credential Manager, per user). Never in a file, settings
 * or the database. Where no store exists, available() is false and the
 * client keeps the session in memory only.
 */
class CredentialStore {
public:
    explicit CredentialStore(QString target);

    bool available() const;
    std::optional<QString> read() const;
    bool write(const QString &secret) const;
    void remove() const;

private:
    QString m_target;
};

} // namespace katsodi
