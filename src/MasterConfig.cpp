#include "MasterConfig.h"
#include "engine/CryptoEngine.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/crypto.h>
#include <windows.h>
#include <dpapi.h>

using namespace bytelock;

namespace {

QString toHex(const std::vector<uint8_t>& bytes)
{
    QByteArray ba(reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()));
    return QString::fromLatin1(ba.toHex());
}

std::vector<uint8_t> fromHex(const QString& hex)
{
    QByteArray ba = QByteArray::fromHex(hex.toLatin1());
    return std::vector<uint8_t>(ba.begin(), ba.end());
}

QJsonObject readSecurityObject()
{
    QFile file(MasterConfig::configPath());
    if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
    return QJsonDocument::fromJson(file.readAll()).object()["security"].toObject();
}

QByteArray dpapiProtect(const std::vector<uint8_t>& plain)
{
    DATA_BLOB in{ static_cast<DWORD>(plain.size()), const_cast<BYTE*>(plain.data()) };
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"ByteLockEscrowKey", nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return QByteArray();
    }
    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

std::vector<uint8_t> dpapiUnprotect(const QByteArray& blob)
{
    DATA_BLOB in{ static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE*>(const_cast<char*>(blob.data())) };
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                             CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return {};
    }
    std::vector<uint8_t> result(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return result;
}

} // namespace

QString MasterConfig::configPath()
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath("config.json");
}

bool MasterConfig::exists()
{
    return QFile::exists(configPath());
}

void MasterConfig::setup(const QString& recoveryKey)
{
    auto verifierSalt = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
    auto keySalt = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
    auto escrowKey = CryptoEngine::randomBytes(CryptoEngine::KeySizeBytes);
    if (!verifierSalt || !keySalt || !escrowKey) return;

    auto verifierHash = CryptoEngine::deriveKey(recoveryKey.toStdString(), verifierSalt.value());
    if (!verifierHash) return;

    QByteArray protectedEscrow = dpapiProtect(escrowKey.value());
    if (protectedEscrow.isEmpty()) return;

    QJsonObject security;
    security["verifier_salt_hex"] = toHex(verifierSalt.value());
    security["verifier_hash_hex"] = toHex(std::vector<uint8_t>(
        verifierHash.value().data(), verifierHash.value().data() + verifierHash.value().size()));
    security["key_salt_hex"] = toHex(keySalt.value());
    security["escrow_key_protected_b64"] = QString::fromLatin1(protectedEscrow.toBase64());

    QJsonObject root;
    root["version"] = "1.0.0";
    root["security"] = security;

    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QFile file(configPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

bool MasterConfig::verify(const QString& recoveryKey)
{
    QJsonObject security = readSecurityObject();
    auto verifierSalt = fromHex(security["verifier_salt_hex"].toString());
    auto storedHash = fromHex(security["verifier_hash_hex"].toString());
    if (verifierSalt.empty() || storedHash.empty()) return false;

    auto computed = CryptoEngine::deriveKey(recoveryKey.toStdString(), verifierSalt);
    if (!computed) return false;

    const auto& c = computed.value();
    if (c.size() != storedHash.size()) return false;

    return CRYPTO_memcmp(c.data(), storedHash.data(), storedHash.size()) == 0;
}

SecureBytes MasterConfig::deriveMasterKey(const QString& recoveryKey)
{
    QJsonObject security = readSecurityObject();
    auto keySalt = fromHex(security["key_salt_hex"].toString());
    if (keySalt.empty()) return SecureBytes();

    auto key = CryptoEngine::deriveKey(recoveryKey.toStdString(), keySalt);
    if (!key) return SecureBytes();

    return std::move(key.value());
}

SecureBytes MasterConfig::getEscrowKey()
{
    QJsonObject security = readSecurityObject();
    QByteArray blob = QByteArray::fromBase64(security["escrow_key_protected_b64"].toString().toLatin1());
    if (blob.isEmpty()) return SecureBytes();

    auto raw = dpapiUnprotect(blob);
    if (raw.empty()) return SecureBytes();

    SecureBytes result(raw.size());
    memcpy(result.data(), raw.data(), raw.size());
    return result;
}

QByteArray MasterConfig::wrapEscrowKeyWithRecoveryKey(const QString& recoveryKey)
{
    SecureBytes escrow = getEscrowKey();
    if (escrow.empty()) return QByteArray();

    auto salt = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
    if (!salt) return QByteArray();

    auto wrapKey = CryptoEngine::deriveKey(recoveryKey.toStdString(), salt.value());
    if (!wrapKey) return QByteArray();

    std::vector<uint8_t> escrowBytes(escrow.data(), escrow.data() + escrow.size());
    auto packed = CryptoEngine::encryptBuffer(escrowBytes, wrapKey.value());
    if (!packed) return QByteArray();

    QByteArray out;
    out.append(reinterpret_cast<const char*>(salt.value().data()), static_cast<int>(salt.value().size()));
    out.append(reinterpret_cast<const char*>(packed.value().data()), static_cast<int>(packed.value().size()));
    return out.toBase64();
}
