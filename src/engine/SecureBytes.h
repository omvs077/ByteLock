#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include <openssl/crypto.h>

namespace bytelock {

class SecureBytes {
public:
    SecureBytes() = default;

    explicit SecureBytes(size_t size) : m_data(size, 0) {}

    explicit SecureBytes(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    ~SecureBytes()
    {
        wipe();
    }

    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;

    SecureBytes(SecureBytes&& other) noexcept
        : m_data(std::move(other.m_data))
    {
    }

    SecureBytes& operator=(SecureBytes&& other) noexcept
    {
        if (this != &other) {
            wipe();
            m_data = std::move(other.m_data);
        }
        return *this;
    }

    uint8_t* data() { return m_data.data(); }
    const uint8_t* data() const { return m_data.data(); }
    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }

    void resize(size_t n) { m_data.resize(n, 0); }

private:
    void wipe()
    {
        if (!m_data.empty()) {
            OPENSSL_cleanse(m_data.data(), m_data.size());
        }
    }

    std::vector<uint8_t> m_data;
};

} // namespace bytelock
