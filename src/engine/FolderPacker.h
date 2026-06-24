#pragma once

#include <cstdint>
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

    static Result<void> lockFolder(const std::string& folderPath,
                                    const std::string& containerPath,
                                    const SecureBytes& key,
                                    const std::vector<uint8_t>& salt,
                                    uint64_t streamingThresholdBytes = DefaultStreamingThresholdBytes);

    static Result<std::vector<uint8_t>> peekContainerSalt(const std::string& containerPath);

    static Result<void> unlockFolder(const std::string& containerPath,
                                      const std::string& destinationFolderPath,
                                      const SecureBytes& key);
};

} // namespace bytelock
