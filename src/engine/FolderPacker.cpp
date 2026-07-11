#include "engine/FolderPacker.h"
#include "engine/CryptoEngine.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace bytelock {

namespace fs = std::filesystem;

namespace {

constexpr char kMagic[4] = {'B', 'L', 'K', 'C'};
constexpr uint8_t kFormatVersion = 1;
constexpr uint8_t kModeSimple = 0;
constexpr uint8_t kModeStreaming = 1;
constexpr size_t kChunkSize = 4 * 1024 * 1024;
constexpr size_t kHeaderSize = 4 + 1 + 1 + CryptoEngine::SaltSizeBytes + CryptoEngine::IvSizeBytes;

struct FileEntry {
    std::string relativePath;
    fs::path absolutePath;
    uint64_t size = 0;
};

struct ManifestFileInfo {
    std::string relativePath;
    uint64_t size = 0;
};

void appendUint16LE(std::vector<uint8_t>& buf, uint16_t v)
{
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void appendUint32LE(std::vector<uint8_t>& buf, uint32_t v)
{
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

void appendUint64LE(std::vector<uint8_t>& buf, uint64_t v)
{
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

bool readUint16LE(const uint8_t* buf, size_t available, size_t offset, uint16_t& outVal)
{
    if (offset + 2 > available) return false;
    outVal = static_cast<uint16_t>(buf[offset]) | (static_cast<uint16_t>(buf[offset + 1]) << 8);
    return true;
}

bool readUint32LE(const uint8_t* buf, size_t available, size_t offset, uint32_t& outVal)
{
    if (offset + 4 > available) return false;
    outVal = 0;
    for (int i = 0; i < 4; ++i) outVal |= static_cast<uint32_t>(buf[offset + i]) << (8 * i);
    return true;
}

bool readUint64LE(const uint8_t* buf, size_t available, size_t offset, uint64_t& outVal)
{
    if (offset + 8 > available) return false;
    outVal = 0;
    for (int i = 0; i < 8; ++i) outVal |= static_cast<uint64_t>(buf[offset + i]) << (8 * i);
    return true;
}

Result<std::vector<FileEntry>> collectFiles(const fs::path& folderPath)
{
    std::vector<FileEntry> entries;
    std::error_code ec;
    if (!fs::exists(folderPath, ec) || !fs::is_directory(folderPath, ec)) {
        return Result<std::vector<FileEntry>>::fail(ErrorCode::FileNotFound, folderPath.string());
    }

    for (auto it = fs::recursive_directory_iterator(folderPath, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            return Result<std::vector<FileEntry>>::fail(ErrorCode::FileReadError, "Error walking directory: " + ec.message());
        }
        if (it->is_regular_file(ec)) {
            FileEntry entry;
            entry.absolutePath = it->path();

            std::error_code relEc;
            fs::path relPath = fs::relative(it->path(), folderPath, relEc);
            if (relEc) {
                return Result<std::vector<FileEntry>>::fail(ErrorCode::FileReadError,
                    "Failed computing relative path for " + it->path().string() + ": " + relEc.message());
            }
            entry.relativePath = relPath.generic_string();

            std::error_code sizeEc;
            entry.size = static_cast<uint64_t>(fs::file_size(it->path(), sizeEc));
            if (sizeEc) {
                return Result<std::vector<FileEntry>>::fail(ErrorCode::FileReadError,
                    "Failed getting file size for " + it->path().string() + ": " + sizeEc.message());
            }

            entries.push_back(std::move(entry));
        }
    }
    return Result<std::vector<FileEntry>>::ok(std::move(entries));
}

std::vector<uint8_t> buildManifestBytes(const std::vector<FileEntry>& entries)
{
    std::vector<uint8_t> manifest;
    appendUint32LE(manifest, static_cast<uint32_t>(entries.size()));
    for (const auto& entry : entries) {
        appendUint16LE(manifest, static_cast<uint16_t>(entry.relativePath.size()));
        manifest.insert(manifest.end(), entry.relativePath.begin(), entry.relativePath.end());
        appendUint64LE(manifest, entry.size);
    }
    return manifest;
}

bool tryParseManifestPrefix(const std::vector<uint8_t>& buf,
                             std::vector<ManifestFileInfo>& outEntries,
                             size_t& consumedBytes)
{
    constexpr uint32_t kMaxPlausibleFileCount = 1000000;

    size_t offset = 0;
    uint32_t fileCount = 0;
    if (!readUint32LE(buf.data(), buf.size(), offset, fileCount)) return false;
    if (fileCount > kMaxPlausibleFileCount) return false;
    offset += 4;

    std::vector<ManifestFileInfo> entries;
    entries.reserve(fileCount);

    for (uint32_t i = 0; i < fileCount; ++i) {
        uint16_t pathLen = 0;
        if (!readUint16LE(buf.data(), buf.size(), offset, pathLen)) return false;
        offset += 2;
        if (offset + pathLen > buf.size()) return false;
        std::string relPath(reinterpret_cast<const char*>(buf.data() + offset), pathLen);
        offset += pathLen;
        uint64_t fileSize = 0;
        if (!readUint64LE(buf.data(), buf.size(), offset, fileSize)) return false;
        offset += 8;
        entries.push_back({std::move(relPath), fileSize});
    }

    outEntries = std::move(entries);
    consumedBytes = offset;
    return true;
}

} // namespace

Result<void> FolderPacker::lockFolder(const std::string& folderPath,
                                       const std::string& containerPath,
                                       const SecureBytes& key,
                                       const std::vector<uint8_t>& salt,
                                       uint64_t streamingThresholdBytes,
                                       const ProgressCallback& onProgress)
{
    try {
        if (salt.size() != CryptoEngine::SaltSizeBytes) {
            return Result<void>::fail(ErrorCode::InvalidInput, "Salt must be 16 bytes");
        }

        fs::path folder(folderPath);
        auto filesResult = collectFiles(folder);
        if (!filesResult) {
            return Result<void>::fail(filesResult.error(), filesResult.detail());
        }
        const auto& entries = filesResult.value();

        uint64_t totalContentBytes = 0;
        for (const auto& e : entries) totalContentBytes += e.size;

        std::vector<uint8_t> manifest = buildManifestBytes(entries);
        const bool useStreaming = (manifest.size() + totalContentBytes) > streamingThresholdBytes;

        std::ofstream out(containerPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Result<void>::fail(ErrorCode::FileWriteError, containerPath);
        }

        out.write(kMagic, sizeof(kMagic));
        out.put(static_cast<char>(kFormatVersion));
        out.put(static_cast<char>(useStreaming ? kModeStreaming : kModeSimple));
        out.write(reinterpret_cast<const char*>(salt.data()), static_cast<std::streamsize>(salt.size()));

        if (!useStreaming) {
            std::vector<uint8_t> blob = manifest;
            blob.reserve(manifest.size() + totalContentBytes);

            for (const auto& entry : entries) {
                std::ifstream in(entry.absolutePath, std::ios::binary);
                if (!in) {
                    return Result<void>::fail(ErrorCode::FileReadError, entry.absolutePath.string());
                }
                blob.insert(blob.end(), std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }

            auto packed = CryptoEngine::encryptBuffer(blob, key);
            if (!packed) {
                return Result<void>::fail(packed.error(), packed.detail());
            }

            out.write(reinterpret_cast<const char*>(packed.value().data()), static_cast<std::streamsize>(packed.value().size()));
            if (!out) {
                return Result<void>::fail(ErrorCode::FileWriteError, "Failed writing container (simple mode)");
            }
        } else {
            auto encryptorResult = GcmStreamEncryptor::create(key);
            if (!encryptorResult) {
                return Result<void>::fail(encryptorResult.error(), encryptorResult.detail());
            }
            auto& encryptor = *encryptorResult.value();

            out.write(reinterpret_cast<const char*>(encryptor.iv().data()), static_cast<std::streamsize>(encryptor.iv().size()));

            auto writeChunk = [&](const uint8_t* data, size_t len) -> Result<void> {
                auto cipherChunk = encryptor.update(data, len);
                if (!cipherChunk) {
                    return Result<void>::fail(cipherChunk.error(), cipherChunk.detail());
                }
                out.write(reinterpret_cast<const char*>(cipherChunk.value().data()), static_cast<std::streamsize>(cipherChunk.value().size()));
                if (!out) {
                    return Result<void>::fail(ErrorCode::FileWriteError, "Failed writing container (streaming mode)");
                }
                return Result<void>::ok();
            };

            if (auto r = writeChunk(manifest.data(), manifest.size()); !r) return r;

            uint64_t bytesDone = 0;
            for (const auto& entry : entries) {
                std::ifstream in(entry.absolutePath, std::ios::binary);
                if (!in) {
                    return Result<void>::fail(ErrorCode::FileReadError, entry.absolutePath.string());
                }
                std::vector<uint8_t> readBuf(kChunkSize);
                while (in) {
                    in.read(reinterpret_cast<char*>(readBuf.data()), static_cast<std::streamsize>(readBuf.size()));
                    std::streamsize got = in.gcount();
                    if (got <= 0) break;
                    if (auto r = writeChunk(readBuf.data(), static_cast<size_t>(got)); !r) return r;
                    bytesDone += static_cast<uint64_t>(got);
                    if (onProgress) onProgress(bytesDone, totalContentBytes);
                }
            }

            auto tagResult = encryptor.finish();
            if (!tagResult) {
                return Result<void>::fail(tagResult.error(), tagResult.detail());
            }
            out.write(reinterpret_cast<const char*>(tagResult.value().data()), static_cast<std::streamsize>(tagResult.value().size()));
            if (!out) {
                return Result<void>::fail(ErrorCode::FileWriteError, "Failed writing container tag (streaming mode)");
            }
        }

        out.close();

        std::error_code ec;
        fs::remove_all(folder, ec);
        if (ec) {
            return Result<void>::fail(ErrorCode::FileWriteError,
                "Container written successfully, but failed to remove original folder: " + ec.message());
        }

        return Result<void>::ok();
    } catch (const std::exception& e) {
        return Result<void>::fail(ErrorCode::EncryptionFailed, std::string("Unexpected exception during lock: ") + e.what());
    }
}

Result<std::vector<uint8_t>> FolderPacker::peekContainerSalt(const std::string& containerPath)
{
    std::ifstream in(containerPath, std::ios::binary);
    if (!in) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::FileNotFound, containerPath);
    }

    char magic[4];
    in.read(magic, 4);
    if (!in || std::memcmp(magic, kMagic, 4) != 0) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::InvalidInput, "Not a valid ByteLock container");
    }

    in.ignore(2);

    std::vector<uint8_t> salt(CryptoEngine::SaltSizeBytes);
    in.read(reinterpret_cast<char*>(salt.data()), static_cast<std::streamsize>(salt.size()));
    if (!in) {
        return Result<std::vector<uint8_t>>::fail(ErrorCode::FileReadError, "Container too small to contain a valid header");
    }

    return Result<std::vector<uint8_t>>::ok(std::move(salt));
}

Result<void> FolderPacker::unlockFolder(const std::string& containerPath,
                                         const std::string& destinationFolderPath,
                                         const SecureBytes& key,
                                         const ProgressCallback& onProgress)
{
    try {
        fs::path destination(destinationFolderPath);
        std::error_code existsEc;
        if (fs::exists(destination, existsEc)) {
            return Result<void>::fail(ErrorCode::InvalidInput, "Destination folder already exists: " + destination.string());
        }

        std::error_code sizeEc;
        uint64_t fileSize = static_cast<uint64_t>(fs::file_size(containerPath, sizeEc));
        if (sizeEc) {
            return Result<void>::fail(ErrorCode::FileNotFound, containerPath);
        }
        if (fileSize < kHeaderSize + CryptoEngine::TagSizeBytes) {
            return Result<void>::fail(ErrorCode::InvalidInput, "Container file too small to be valid");
        }

        std::ifstream in(containerPath, std::ios::binary);
        if (!in) {
            return Result<void>::fail(ErrorCode::FileNotFound, containerPath);
        }

        char magic[4];
        in.read(magic, 4);
        if (!in || std::memcmp(magic, kMagic, 4) != 0) {
            return Result<void>::fail(ErrorCode::InvalidInput, "Not a valid ByteLock container");
        }

        in.ignore(1);
        in.ignore(1);

        std::vector<uint8_t> salt(CryptoEngine::SaltSizeBytes);
        in.read(reinterpret_cast<char*>(salt.data()), static_cast<std::streamsize>(salt.size()));

        std::vector<uint8_t> iv(CryptoEngine::IvSizeBytes);
        in.read(reinterpret_cast<char*>(iv.data()), static_cast<std::streamsize>(iv.size()));

        if (!in) {
            return Result<void>::fail(ErrorCode::FileReadError, "Failed reading container header");
        }

        const uint64_t ciphertextLen = fileSize - kHeaderSize - CryptoEngine::TagSizeBytes;

        std::vector<uint8_t> tag(CryptoEngine::TagSizeBytes);
        in.seekg(static_cast<std::streamoff>(fileSize - CryptoEngine::TagSizeBytes));
        in.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
        if (!in) {
            return Result<void>::fail(ErrorCode::FileReadError, "Failed reading container tag");
        }
        in.seekg(static_cast<std::streamoff>(kHeaderSize));

        auto decryptorResult = GcmStreamDecryptor::create(key, iv);
        if (!decryptorResult) {
            return Result<void>::fail(decryptorResult.error(), decryptorResult.detail());
        }
        auto& decryptor = *decryptorResult.value();

        fs::path stagingDir = destination;
        stagingDir += ".unlocking_tmp";

        std::error_code cleanupEc;
        fs::remove_all(stagingDir, cleanupEc);
        fs::create_directories(stagingDir, cleanupEc);

        std::vector<uint8_t> manifestBuffer;
        bool manifestParsed = false;
        std::vector<ManifestFileInfo> entries;
        size_t currentFileIndex = 0;
        uint64_t currentFileBytesWritten = 0;
        std::ofstream currentOutputFile;

        auto failAndCleanup = [&](ErrorCode code, const std::string& detail) -> Result<void> {
            if (currentOutputFile.is_open()) currentOutputFile.close();
            if (in.is_open()) in.close();
            std::error_code ec;
            fs::remove_all(stagingDir, ec);
            return Result<void>::fail(code, detail);
        };

        auto writeToCurrentFile = [&](const uint8_t* data, size_t len) -> Result<void> {
            size_t offset = 0;
            while (offset < len && currentFileIndex < entries.size()) {
                if (!currentOutputFile.is_open()) {
                    fs::path outPath = stagingDir / fs::path(entries[currentFileIndex].relativePath);
                    std::error_code dirEc;
                    fs::create_directories(outPath.parent_path(), dirEc);
                    currentOutputFile.open(outPath, std::ios::binary | std::ios::trunc);
                    if (!currentOutputFile) {
                        return Result<void>::fail(ErrorCode::FileWriteError, outPath.string());
                    }
                    currentFileBytesWritten = 0;
                }
                uint64_t needed = entries[currentFileIndex].size - currentFileBytesWritten;
                size_t available = len - offset;
                size_t writeNow = static_cast<size_t>(std::min<uint64_t>(needed, available));
                currentOutputFile.write(reinterpret_cast<const char*>(data + offset), static_cast<std::streamsize>(writeNow));
                currentFileBytesWritten += writeNow;
                offset += writeNow;
                if (currentFileBytesWritten == entries[currentFileIndex].size) {
                    currentOutputFile.close();
                    ++currentFileIndex;
                    currentFileBytesWritten = 0;
                }
            }
            return Result<void>::ok();
        };

        uint64_t remainingCiphertext = ciphertextLen;
        std::vector<uint8_t> readBuf(kChunkSize);

        while (remainingCiphertext > 0) {
            size_t toRead = static_cast<size_t>(std::min<uint64_t>(remainingCiphertext, kChunkSize));
            in.read(reinterpret_cast<char*>(readBuf.data()), static_cast<std::streamsize>(toRead));
            if (!in) {
                return failAndCleanup(ErrorCode::FileReadError, "Failed reading container ciphertext");
            }
            remainingCiphertext -= toRead;
            if (onProgress) onProgress(ciphertextLen - remainingCiphertext, ciphertextLen);

            auto plainResult = decryptor.update(readBuf.data(), toRead);
            if (!plainResult) {
                return failAndCleanup(plainResult.error(), plainResult.detail());
            }
            const std::vector<uint8_t>& plain = plainResult.value();

            if (!manifestParsed) {
                manifestBuffer.insert(manifestBuffer.end(), plain.begin(), plain.end());
                size_t consumed = 0;
                if (tryParseManifestPrefix(manifestBuffer, entries, consumed)) {
                    manifestParsed = true;
                    std::vector<uint8_t> leftover(manifestBuffer.begin() + static_cast<long>(consumed), manifestBuffer.end());
                    manifestBuffer.clear();
                    manifestBuffer.shrink_to_fit();
                    if (auto r = writeToCurrentFile(leftover.data(), leftover.size()); !r) {
                        return failAndCleanup(r.error(), r.detail());
                    }
                }
                continue;
            }

            if (auto r = writeToCurrentFile(plain.data(), plain.size()); !r) {
                return failAndCleanup(r.error(), r.detail());
            }
        }

        if (currentOutputFile.is_open()) currentOutputFile.close();

        auto finishResult = decryptor.finish(tag);
        if (!finishResult) {
            return failAndCleanup(finishResult.error(), finishResult.detail());
        }

        if (!manifestParsed || currentFileIndex != entries.size()) {
            return failAndCleanup(ErrorCode::DecryptionFailed,
                "Internal error: container parsed inconsistently despite valid authentication");
        }

        in.close();

        std::error_code renameEc;
        fs::rename(stagingDir, destination, renameEc);
        if (renameEc) {
            std::error_code ec;
            fs::remove_all(stagingDir, ec);
            return Result<void>::fail(ErrorCode::FileWriteError, "Failed to finalize unlocked folder: " + renameEc.message());
        }

        std::error_code removeEc;
        fs::remove(containerPath, removeEc);
        if (removeEc) {
            return Result<void>::fail(ErrorCode::FileWriteError,
                "Folder restored successfully, but could not remove the original container file: " + removeEc.message());
        }

        return Result<void>::ok();
    } catch (const std::exception& e) {
        return Result<void>::fail(ErrorCode::DecryptionFailed, std::string("Unexpected exception during unlock: ") + e.what());
    }
}

} // namespace bytelock





