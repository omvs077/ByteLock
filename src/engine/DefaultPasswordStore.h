#pragma once

#include <string>

#include "engine/Result.h"

namespace bytelock {

class DefaultPasswordStore {
public:
    DefaultPasswordStore();

    bool exists() const;

    Result<void> createNew(const std::string& password);

    Result<bool> verify(const std::string& password) const;

private:
    std::string resolveConfigFilePath() const;

    std::string m_configPath;
};

} // namespace bytelock
