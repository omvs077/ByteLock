#include "engine/DefaultPasswordStore.h"
#include "engine/CryptoEngine.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace bytelock {

namespace fs = std::filesystem;

namespace {
constexpr char kMagic[4] = {'B', 'L', 'K', 'P'};
constexpr uint8_t kFormatVersion = 1;
const std::string kVerifierPlaintext = "BYTELOCK_DEFAULT_PASSWORD_VERIFIER_V1";
}

DefaultPasswordStore::DefaultPasswordStore()
{
    m_configPath = resolveConfigFilePath();
}

std::string DefaultPasswordStore::resolveConfigFilePath() const
{
    const char* appData = std::getenv("APPDATA");
    fs::path base = appData ? fs::path(appData) : fs::temp_directory_path();
    fs::path dir = base / "ByteLock";

    std::error_code ec;
    fs::create_directories(dir, ec);

    return (dir / "config.bin").string();
}

bool DefaultPasswordStore::exists() const
{
    std::error_code ec;
    return fs::exists(m_configPath, ec);
}

Result<void> DefaultPasswordStore::createNew(const std::string& password)
{
    auto saltResult = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
    if (!saltResult) {
        return Result<void>::fail(saltResult.error(), saltResult.detail());
    }

    auto keyResult = CryptoEngine::deriveKey(password, saltResult.value());
    if (!keyResult) {
        return Result<void>::fail(keyResult.error(), keyResult.detail());
    }

    std::vector<uint8_t> verifierPlain(kVerifierPlaintext.begin(), kVerifierPlaintext.end());
    auto encryptedVerifier = CryptoEngine::encryptBuffer(verifierPlain, keyResult.value());
    if (!encryptedVerifier) {
        return Result<void>::fail(encryptedVerifier.error(), encryptedVerifier.detail());
    }

    std::ofstream out(m_configPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Result<void>::fail(ErrorCode::FileWriteError, m_configPath);
    }

    out.write(kMagic, sizeof(kMagic));
    out.put(static_cast<char>(kFormatVersion));
    out.write(reinterpret_cast<const char*>(saltResult.value().data()),
               static_cast<std::streamsize>(saltResult.value().size()));
    out.write(reinterpret_cast<const char*>(encryptedVerifier.value().data()),
               static_cast<std::streamsize>(encryptedVerifier.value().size()));

    if (!out) {
        return Result<void>::fail(ErrorCode::FileWriteError, "Failed writing config file");
    }

    return Result<void>::ok();
}

Result<bool> DefaultPasswordStore::verify(const std::string& password) const
{
    std::ifstream in(m_configPath, std::ios::binary);
    if (!in) {
        return Result<bool>::fail(ErrorCode::FileNotFound, m_configPath);
    }

    char magic[4];
    in.read(magic, 4);
    if (!in || std::memcmp(magic, kMagic, 4) != 0) {
        return Result<bool>::fail(ErrorCode::InvalidInput, "Not a valid ByteLock config file");
    }

    in.ignore(1);

    std::vector<uint8_t> salt(CryptoEngine::SaltSizeBytes);
    in.read(reinterpret_cast<char*>(salt.data()), static_cast<std::streamsize>(salt.size()));
    if (!in) {
        return Result<bool>::fail(ErrorCode::FileReadError, "Config file too small to be valid");
    }

    std::vector<uint8_t> encryptedVerifier((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    auto keyResult = CryptoEngine::deriveKey(password, salt);
    if (!keyResult) {
        return Result<bool>::fail(keyResult.error(), keyResult.detail());
    }

    auto decrypted = CryptoEngine::decryptBuffer(encryptedVerifier, keyResult.value());
    if (!decrypted) {
        if (decrypted.error() == ErrorCode::AuthenticationFailed) {
            return Result<bool>::ok(false);
        }
        return Result<bool>::fail(decrypted.error(), decrypted.detail());
    }

    std::string verifierText(decrypted.value().begin(), decrypted.value().end());
    return Result<bool>::ok(verifierText == kVerifierPlaintext);
}

} // namespace bytelock
