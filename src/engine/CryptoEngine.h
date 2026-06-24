#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/Result.h"
#include "engine/SecureBytes.h"

namespace bytelock {

struct Argon2Params {
    uint32_t iterations = 3;
    uint32_t memoryCostKb = 65536;
    uint32_t parallelism = 4;
    uint32_t threads = 1;
};

class CryptoEngine {
public:
    static constexpr size_t KeySizeBytes = 32;
    static constexpr size_t SaltSizeBytes = 16;
    static constexpr size_t IvSizeBytes = 12;
    static constexpr size_t TagSizeBytes = 16;

    static Result<std::vector<uint8_t>> randomBytes(size_t count);

    static Result<SecureBytes> deriveKey(const std::string& password,
                                          const std::vector<uint8_t>& salt,
                                          const Argon2Params& params = {});

    static Result<std::vector<uint8_t>> encryptBuffer(const std::vector<uint8_t>& plaintext,
                                                        const SecureBytes& key);

    static Result<std::vector<uint8_t>> decryptBuffer(const std::vector<uint8_t>& packed,
                                                        const SecureBytes& key);

    static Result<void> encryptFile(const std::string& inputPath,
                                     const std::string& outputPath,
                                     const SecureBytes& key);

    static Result<void> decryptFile(const std::string& inputPath,
                                     const std::string& outputPath,
                                     const SecureBytes& key);
};

// Streaming AES-256-GCM encryption for data too large to hold in memory at once.
// Uses the Pimpl idiom so this header never exposes raw OpenSSL types.
class GcmStreamEncryptor {
public:
    static Result<std::unique_ptr<GcmStreamEncryptor>> create(const SecureBytes& key);
    ~GcmStreamEncryptor();

    GcmStreamEncryptor(const GcmStreamEncryptor&) = delete;
    GcmStreamEncryptor& operator=(const GcmStreamEncryptor&) = delete;

    const std::vector<uint8_t>& iv() const { return m_iv; }

    Result<std::vector<uint8_t>> update(const uint8_t* data, size_t len);
    Result<std::vector<uint8_t>> finish();

private:
    GcmStreamEncryptor();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::vector<uint8_t> m_iv;
};

class GcmStreamDecryptor {
public:
    static Result<std::unique_ptr<GcmStreamDecryptor>> create(const SecureBytes& key,
                                                                const std::vector<uint8_t>& iv);
    ~GcmStreamDecryptor();

    GcmStreamDecryptor(const GcmStreamDecryptor&) = delete;
    GcmStreamDecryptor& operator=(const GcmStreamDecryptor&) = delete;

    Result<std::vector<uint8_t>> update(const uint8_t* data, size_t len);
    Result<void> finish(const std::vector<uint8_t>& tag);

private:
    GcmStreamDecryptor();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace bytelock
