#include "FsCrypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>

namespace FsCrypto {

    static uint8_t g_Key[32] = { 0 };

    void SetKeyFromPassword(const wchar_t* password)
    {
        // POC only: SHA-256(utf16 bytes). Replace with Argon2id KDF later.
        size_t len = wcslen(password) * sizeof(wchar_t);
        unsigned int outLen = 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, password, len);
        EVP_DigestFinal_ex(ctx, g_Key, &outLen);
        EVP_MD_CTX_free(ctx);
    }

    uint64_t CipherSizeFromPlainSize(uint64_t plainSize)
    {
        if (0 == plainSize)
            return 0;
        uint64_t fullChunks = plainSize / ChunkSize;
        uint32_t rem = (uint32_t)(plainSize % ChunkSize);
        uint64_t size = fullChunks * (uint64_t)CipherChunkMax;
        if (rem > 0)
            size += rem + ChunkOverhead;
        return size;
    }

    uint64_t PlainSizeFromCipherSize(uint64_t cipherSize)
    {
        if (0 == cipherSize)
            return 0;
        uint64_t fullChunks = cipherSize / CipherChunkMax;
        uint32_t rem = (uint32_t)(cipherSize % CipherChunkMax);
        uint64_t size = fullChunks * (uint64_t)ChunkSize;
        if (rem > 0)
            size += rem - ChunkOverhead;
        return size;
    }

    uint64_t ChunkCipherOffset(uint64_t chunkIndex)
    {
        return chunkIndex * (uint64_t)CipherChunkMax;
    }

    uint32_t PlainChunkLen(uint64_t plainFileSize, uint64_t chunkIndex)
    {
        uint64_t start = chunkIndex * (uint64_t)ChunkSize;
        if (start >= plainFileSize)
            return 0;
        uint64_t remaining = plainFileSize - start;
        return (uint32_t)(remaining < ChunkSize ? remaining : ChunkSize);
    }

    bool EncryptChunk(const uint8_t* plain, uint32_t plainLen, uint8_t* cipherOut)
    {
        uint8_t* nonce = cipherOut;
        uint8_t* ct = cipherOut + 12;
        uint8_t* tag = cipherOut + 12 + plainLen;

        if (1 != RAND_bytes(nonce, 12))
            return false;

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            return false;

        bool ok = true;
        int len = 0;
        ok &= 1 == EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        ok &= 1 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        ok &= 1 == EVP_EncryptInit_ex(ctx, nullptr, nullptr, g_Key, nonce);
        if (plainLen > 0)
            ok &= 1 == EVP_EncryptUpdate(ctx, ct, &len, plain, (int)plainLen);
        int finalLen = 0;
        ok &= 1 == EVP_EncryptFinal_ex(ctx, ct + len, &finalLen);
        ok &= 1 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);

        EVP_CIPHER_CTX_free(ctx);
        return ok;
    }

    bool DecryptChunk(const uint8_t* cipher, uint32_t cipherLen, uint8_t* plainOut)
    {
        if (cipherLen < ChunkOverhead)
            return false;
        uint32_t plainLen = cipherLen - ChunkOverhead;
        const uint8_t* nonce = cipher;
        const uint8_t* ct = cipher + 12;
        const uint8_t* tag = cipher + 12 + plainLen;

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            return false;

        bool ok = true;
        int len = 0;
        ok &= 1 == EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        ok &= 1 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        ok &= 1 == EVP_DecryptInit_ex(ctx, nullptr, nullptr, g_Key, nonce);
        if (plainLen > 0)
            ok &= 1 == EVP_DecryptUpdate(ctx, plainOut, &len, ct, (int)plainLen);
        ok &= 1 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag);
        int finalLen = 0;
        ok &= 1 == EVP_DecryptFinal_ex(ctx, plainOut + len, &finalLen);

        EVP_CIPHER_CTX_free(ctx);
        return ok;
    }

} // namespace FsCrypto