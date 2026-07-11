#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/Result.h"
#include "engine/SecureBytes.h"

namespace bytelock {

// Encrypts an entire folder (recursively) into a single opaque container file,
// and reverses the process to reconstruct the folder exactly as it was.
//
// Known limitation: empty subdirectories (containing zero files) are not
// preserved across a lock/unlock round-trip - only files are tracked.
class FolderPacker {
public:
    static constexpr uint64_t DefaultStreamingThresholdBytes = 256ull * 1024 * 1024;

    using ProgressCallback = std::function<void(uint64_t done, uint64_t total)>;

    static Result<void> lockFolder(const std::string& folderPath,
                                    const std::string& containerPath,
                                    const SecureBytes& key,
                                    const std::vector<uint8_t>& salt,
                                    uint64_t streamingThresholdBytes = DefaultStreamingThresholdBytes,
                                    const ProgressCallback& onProgress = nullptr);

    static Result<std::vector<uint8_t>> peekContainerSalt(const std::string& containerPath);

    static Result<void> unlockFolder(const std::string& containerPath,
                                      const std::string& destinationFolderPath,
                                      const SecureBytes& key,
                                      const ProgressCallback& onProgress = nullptr);
};

} // namespace bytelock


