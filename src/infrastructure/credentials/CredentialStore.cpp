#include "infrastructure/credentials/CredentialStore.h"

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wincred.h>
#endif

namespace katsodi {

CredentialStore::CredentialStore(QString target) : m_target(std::move(target)) {}

#ifdef Q_OS_WIN

bool CredentialStore::available() const { return true; }

std::optional<QString> CredentialStore::read() const
{
    PCREDENTIALW credential = nullptr;
    const std::wstring target = m_target.toStdWString();
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
        return std::nullopt;
    std::optional<QString> result;
    if (credential->CredentialBlobSize > 0 && credential->CredentialBlobSize <= 8192)
        result = QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob),
                                   static_cast<qsizetype>(credential->CredentialBlobSize));
    CredFree(credential);
    return result;
}

bool CredentialStore::write(const QString &secret) const
{
    QByteArray blob = secret.toUtf8();
    if (blob.isEmpty() || blob.size() > 8192)
        return false;
    std::wstring target = m_target.toStdWString();
    std::wstring user = L"refresh-token";
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target.data();
    credential.UserName = user.data();
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    const bool ok = CredWriteW(&credential, 0) != FALSE;
    blob.fill('\0');
    return ok;
}

void CredentialStore::remove() const
{
    const std::wstring target = m_target.toStdWString();
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

#else

bool CredentialStore::available() const { return false; }
std::optional<QString> CredentialStore::read() const { return std::nullopt; }
bool CredentialStore::write(const QString &) const { return false; }
void CredentialStore::remove() const {}

#endif

} // namespace katsodi
