#include "engine/CryptoEngine.h"

#include <fstream>

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

namespace bytelock {

namespace {
std::string lastOpenSslError()
{
    unsigned long code = ERR_get_error();
    if (code == 0) {
        return "no OpenSSL error queued";
    }
    char buf[256];
    ERR_error_string_n(code, buf, sizeof(buf));
    return std::string(buf);
}
} // namespace

Result<std::vector<uint8_t>> CryptoEngine::randomBytes(size_t count)
{
    std::vector<uint8_t> buf(count);
    if (RAND_bytes(buf.data(), static_cast<int>(count)) != 1) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::OpenSslInitFailed, "RAND_bytes failed");
    }
    return Result<std::vector<uint8_t>>::ok(std::move(buf));
}

Result<SecureBytes> CryptoEngine::deriveKey(const std::string& password,
                                             const std::vector<uint8_t>& salt,
                                             const Argon2Params& params)
{
    ERR_clear_error();

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (!kdf) {
        return Result<SecureBytes>::fail(ErrorCode::KeyDerivationFailed,
            "ARGON2ID KDF not available in this OpenSSL build (requires OpenSSL 3.2+): " + lastOpenSslError());
    }

    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx) {
        return Result<SecureBytes>::fail(ErrorCode::KeyDerivationFailed,
            "Failed to create KDF context: " + lastOpenSslError());
    }

    uint32_t iter = params.iterations;
    uint32_t memcost = params.memoryCostKb;
    uint32_t lanes = params.parallelism;
    uint32_t threads = params.threads;

    OSSL_PARAM ossl_params[7];
    size_t p = 0;

    ossl_params[p++] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD,
        const_cast<char*>(password.data()),
        password.size());

    ossl_params[p++] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SALT,
        const_cast<uint8_t*>(salt.data()),
        salt.size());

    ossl_params[p++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iter);
    ossl_params[p++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memcost);
    ossl_params[p++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes);
    ossl_params[p++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads);
    ossl_params[p++] = OSSL_PARAM_construct_end();

    SecureBytes key(CryptoEngine::KeySizeBytes);

    int rc = EVP_KDF_derive(kctx, key.data(), key.size(), ossl_params);
    EVP_KDF_CTX_free(kctx);

    if (rc <= 0) {
        return Result<SecureBytes>::fail(ErrorCode::KeyDerivationFailed,
            "EVP_KDF_derive failed: " + lastOpenSslError());
    }

    return Result<SecureBytes>::ok(std::move(key));
}

Result<std::vector<uint8_t>> CryptoEngine::encryptBuffer(const std::vector<uint8_t>& plaintext,
                                                          const SecureBytes& key)
{
    if (key.size() != KeySizeBytes) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::InvalidInput, "Key must be 32 bytes (AES-256)");
    }

    auto ivResult = randomBytes(IvSizeBytes);
    if (!ivResult) {
        return Result<std::vector<uint8_t>>::fail(ivResult.error(), ivResult.detail());
    }
    const std::vector<uint8_t>& iv = ivResult.value();

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::EncryptionFailed, "Failed to create cipher context");
    }

    bool failed = false;
    std::vector<uint8_t> ciphertext(plaintext.size());
    int outLen = 0;
    int totalLen = 0;
    std::vector<uint8_t> tag(TagSizeBytes);

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        failed = true;
    }
    if (!failed && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IvSizeBytes), nullptr) != 1) {
        failed = true;
    }
    if (!failed && EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        failed = true;
    }
    if (!failed && !plaintext.empty()) {
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen, plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
            failed = true;
        } else {
            totalLen += outLen;
        }
    }
    if (!failed) {
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + totalLen, &outLen) != 1) {
            failed = true;
        } else {
            totalLen += outLen;
        }
    }
    if (!failed && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TagSizeBytes), tag.data()) != 1) {
        failed = true;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (failed) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::EncryptionFailed, "AES-256-GCM encryption failed");
    }

    ciphertext.resize(totalLen);

    std::vector<uint8_t> packed;
    packed.reserve(IvSizeBytes + ciphertext.size() + TagSizeBytes);
    packed.insert(packed.end(), iv.begin(), iv.end());
    packed.insert(packed.end(), ciphertext.begin(), ciphertext.end());
    packed.insert(packed.end(), tag.begin(), tag.end());

    return Result<std::vector<uint8_t>>::ok(std::move(packed));
}

Result<std::vector<uint8_t>> CryptoEngine::decryptBuffer(const std::vector<uint8_t>& packed,
                                                          const SecureBytes& key)
{
    if (key.size() != KeySizeBytes) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::InvalidInput, "Key must be 32 bytes (AES-256)");
    }
    if (packed.size() < IvSizeBytes + TagSizeBytes) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::InvalidInput, "Encrypted data too small to be valid");
    }

    const uint8_t* iv = packed.data();
    const uint8_t* cipherStart = packed.data() + IvSizeBytes;
    const size_t cipherLen = packed.size() - IvSizeBytes - TagSizeBytes;
    const uint8_t* tag = packed.data() + IvSizeBytes + cipherLen;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::DecryptionFailed, "Failed to create cipher context");
    }

    bool failed = false;
    std::vector<uint8_t> plaintext(cipherLen);
    int outLen = 0;
    int totalLen = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        failed = true;
    }
    if (!failed && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IvSizeBytes), nullptr) != 1) {
        failed = true;
    }
    if (!failed && EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1) {
        failed = true;
    }
    if (!failed && cipherLen > 0) {
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &outLen, cipherStart, static_cast<int>(cipherLen)) != 1) {
            failed = true;
        } else {
            totalLen += outLen;
        }
    }
    if (!failed && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(TagSizeBytes), const_cast<uint8_t*>(tag)) != 1) {
        failed = true;
    }

    bool authOk = false;
    if (!failed) {
        authOk = (EVP_DecryptFinal_ex(ctx, plaintext.data() + totalLen, &outLen) == 1);
        if (authOk) {
            totalLen += outLen;
        }
    }

    EVP_CIPHER_CTX_free(ctx);

    if (failed) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::DecryptionFailed, "AES-256-GCM decryption setup failed");
    }
    if (!authOk) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::AuthenticationFailed,
            "Authentication tag mismatch - wrong key or corrupted/tampered data");
    }

    plaintext.resize(totalLen);
    return Result<std::vector<uint8_t>>::ok(std::move(plaintext));
}

Result<void> CryptoEngine::encryptFile(const std::string& inputPath,
                                        const std::string& outputPath,
                                        const SecureBytes& key)
{
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        return Result<void>::fail(ErrorCode::FileNotFound, inputPath);
    }
    std::vector<uint8_t> plaintext((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    auto encrypted = encryptBuffer(plaintext, key);
    if (!encrypted) {
        return Result<void>::fail(encrypted.error(), encrypted.detail());
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Result<void>::fail(ErrorCode::FileWriteError, outputPath);
    }
    out.write(reinterpret_cast<const char*>(encrypted.value().data()), static_cast<std::streamsize>(encrypted.value().size()));
    if (!out) {
        return Result<void>::fail(ErrorCode::FileWriteError, "Write stream failed");
    }
    return Result<void>::ok();
}

Result<void> CryptoEngine::decryptFile(const std::string& inputPath,
                                        const std::string& outputPath,
                                        const SecureBytes& key)
{
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        return Result<void>::fail(ErrorCode::FileNotFound, inputPath);
    }
    std::vector<uint8_t> packed((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    auto decrypted = decryptBuffer(packed, key);
    if (!decrypted) {
        return Result<void>::fail(decrypted.error(), decrypted.detail());
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Result<void>::fail(ErrorCode::FileWriteError, outputPath);
    }
    out.write(reinterpret_cast<const char*>(decrypted.value().data()), static_cast<std::streamsize>(decrypted.value().size()));
    if (!out) {
        return Result<void>::fail(ErrorCode::FileWriteError, "Write stream failed");
    }
    return Result<void>::ok();
}

// ===================== Streaming GCM encryptor/decryptor =====================

struct GcmStreamEncryptor::Impl {
    EVP_CIPHER_CTX* ctx = nullptr;
    ~Impl() { if (ctx) EVP_CIPHER_CTX_free(ctx); }
};

GcmStreamEncryptor::GcmStreamEncryptor() : m_impl(std::make_unique<Impl>()) {}
GcmStreamEncryptor::~GcmStreamEncryptor() = default;

Result<std::unique_ptr<GcmStreamEncryptor>> GcmStreamEncryptor::create(const SecureBytes& key)
{
    if (key.size() != CryptoEngine::KeySizeBytes) {
        return Result<std::unique_ptr<GcmStreamEncryptor>>::fail(ErrorCode::InvalidInput, "Key must be 32 bytes (AES-256)");
    }

    auto ivResult = CryptoEngine::randomBytes(CryptoEngine::IvSizeBytes);
    if (!ivResult) {
        return Result<std::unique_ptr<GcmStreamEncryptor>>::fail(ivResult.error(), ivResult.detail());
    }

    auto encryptor = std::unique_ptr<GcmStreamEncryptor>(new GcmStreamEncryptor());
    encryptor->m_iv = ivResult.value();
    encryptor->m_impl->ctx = EVP_CIPHER_CTX_new();
    if (!encryptor->m_impl->ctx) {
        return Result<std::unique_ptr<GcmStreamEncryptor>>::fail(ErrorCode::EncryptionFailed, "Failed to create cipher context");
    }

    if (EVP_EncryptInit_ex(encryptor->m_impl->ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(encryptor->m_impl->ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(CryptoEngine::IvSizeBytes), nullptr) != 1
        || EVP_EncryptInit_ex(encryptor->m_impl->ctx, nullptr, nullptr, key.data(), encryptor->m_iv.data()) != 1) {
        return Result<std::unique_ptr<GcmStreamEncryptor>>::fail(ErrorCode::EncryptionFailed, "Failed to initialize GCM stream encryptor");
    }

    return Result<std::unique_ptr<GcmStreamEncryptor>>::ok(std::move(encryptor));
}

Result<std::vector<uint8_t>> GcmStreamEncryptor::update(const uint8_t* data, size_t len)
{
    std::vector<uint8_t> out(len);
    int outLen = 0;
    if (len > 0) {
        if (EVP_EncryptUpdate(m_impl->ctx, out.data(), &outLen, data, static_cast<int>(len)) != 1) {
            return Result<std::vector<uint8_t>>::fail(ErrorCode::EncryptionFailed, "Stream encrypt update failed");
        }
    }
    out.resize(outLen);
    return Result<std::vector<uint8_t>>::ok(std::move(out));
}

Result<std::vector<uint8_t>> GcmStreamEncryptor::finish()
{
    std::vector<uint8_t> finalBuf(16);
    int finalLen = 0;
    if (EVP_EncryptFinal_ex(m_impl->ctx, finalBuf.data(), &finalLen) != 1) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::EncryptionFailed, "Stream encrypt finalize failed");
    }
    std::vector<uint8_t> tag(CryptoEngine::TagSizeBytes);
    if (EVP_CIPHER_CTX_ctrl(m_impl->ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(CryptoEngine::TagSizeBytes), tag.data()) != 1) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::EncryptionFailed, "Failed to retrieve GCM tag");
    }
    return Result<std::vector<uint8_t>>::ok(std::move(tag));
}

struct GcmStreamDecryptor::Impl {
    EVP_CIPHER_CTX* ctx = nullptr;
    ~Impl() { if (ctx) EVP_CIPHER_CTX_free(ctx); }
};

GcmStreamDecryptor::GcmStreamDecryptor() : m_impl(std::make_unique<Impl>()) {}
GcmStreamDecryptor::~GcmStreamDecryptor() = default;

Result<std::unique_ptr<GcmStreamDecryptor>> GcmStreamDecryptor::create(const SecureBytes& key,
                                                                        const std::vector<uint8_t>& iv)
{
    if (key.size() != CryptoEngine::KeySizeBytes) {
        return Result<std::unique_ptr<GcmStreamDecryptor>>::fail(ErrorCode::InvalidInput, "Key must be 32 bytes (AES-256)");
    }
    if (iv.size() != CryptoEngine::IvSizeBytes) {
        return Result<std::unique_ptr<GcmStreamDecryptor>>::fail(ErrorCode::InvalidInput, "IV must be 12 bytes");
    }

    auto decryptor = std::unique_ptr<GcmStreamDecryptor>(new GcmStreamDecryptor());
    decryptor->m_impl->ctx = EVP_CIPHER_CTX_new();
    if (!decryptor->m_impl->ctx) {
        return Result<std::unique_ptr<GcmStreamDecryptor>>::fail(ErrorCode::DecryptionFailed, "Failed to create cipher context");
    }

    if (EVP_DecryptInit_ex(decryptor->m_impl->ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(decryptor->m_impl->ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(CryptoEngine::IvSizeBytes), nullptr) != 1
        || EVP_DecryptInit_ex(decryptor->m_impl->ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        return Result<std::unique_ptr<GcmStreamDecryptor>>::fail(ErrorCode::DecryptionFailed, "Failed to initialize GCM stream decryptor");
    }

    return Result<std::unique_ptr<GcmStreamDecryptor>>::ok(std::move(decryptor));
}

Result<std::vector<uint8_t>> GcmStreamDecryptor::update(const uint8_t* data, size_t len)
{
    std::vector<uint8_t> out(len);
    int outLen = 0;
    if (len > 0) {
        if (EVP_DecryptUpdate(m_impl->ctx, out.data(), &outLen, data, static_cast<int>(len)) != 1) {
            return Result<std::vector<uint8_t>>::fail(ErrorCode::DecryptionFailed, "Stream decrypt update failed");
        }
    }
    out.resize(outLen);
    return Result<std::vector<uint8_t>>::ok(std::move(out));
}

Result<void> GcmStreamDecryptor::finish(const std::vector<uint8_t>& tag)
{
    if (tag.size() != CryptoEngine::TagSizeBytes) {
        return Result<void>::fail(ErrorCode::InvalidInput, "Tag must be 16 bytes");
    }
    if (EVP_CIPHER_CTX_ctrl(m_impl->ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(CryptoEngine::TagSizeBytes),
            const_cast<uint8_t*>(tag.data())) != 1) {
        return Result<void>::fail(ErrorCode::DecryptionFailed, "Failed to set GCM tag");
    }
    unsigned char finalBuf[16];
    int finalLen = 0;
    if (EVP_DecryptFinal_ex(m_impl->ctx, finalBuf, &finalLen) != 1) {
        return Result<void>::fail(ErrorCode::AuthenticationFailed,
            "Authentication tag mismatch - wrong key or corrupted/tampered data");
    }
    return Result<void>::ok();
}

} // namespace bytelock
