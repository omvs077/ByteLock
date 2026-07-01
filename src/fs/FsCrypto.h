#pragma once
#include <windows.h>
#include <cstdint>

namespace FsCrypto {

	constexpr uint32_t ChunkSize = 65536;      // plaintext bytes per chunk
	constexpr uint32_t ChunkOverhead = 28;     // 12 nonce + 16 tag
	constexpr uint32_t CipherChunkMax = ChunkSize + ChunkOverhead;

	void SetKeyFromPassword(const wchar_t* password);

	// size conversions
	uint64_t PlainSizeFromCipherSize(uint64_t cipherSize);
	uint64_t CipherSizeFromPlainSize(uint64_t plainSize);

	// chunk index/offset helpers
	uint64_t ChunkCipherOffset(uint64_t chunkIndex);
	uint32_t PlainChunkLen(uint64_t plainFileSize, uint64_t chunkIndex);

	// encrypt exactly plainLen bytes -> cipherOut must have plainLen+28 bytes. returns false on failure.
	bool EncryptChunk(const uint8_t* plain, uint32_t plainLen, uint8_t* cipherOut);

	// decrypt exactly cipherLen bytes -> plainOut must have cipherLen-28 bytes. returns false on failure.
	bool DecryptChunk(const uint8_t* cipher, uint32_t cipherLen, uint8_t* plainOut);

} // namespace FsCrypto