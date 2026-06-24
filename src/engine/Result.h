#pragma once

#include <string>
#include <utility>

namespace bytelock {

enum class ErrorCode {
    None = 0,
    OpenSslInitFailed,
    KeyDerivationFailed,
    EncryptionFailed,
    DecryptionFailed,
    AuthenticationFailed,
    FileNotFound,
    FileReadError,
    FileWriteError,
    InvalidInput,
};

inline const char* toString(ErrorCode code)
{
    switch (code) {
        case ErrorCode::None:                 return "No error";
        case ErrorCode::OpenSslInitFailed:    return "Failed to initialize OpenSSL primitive";
        case ErrorCode::KeyDerivationFailed:  return "Key derivation (Argon2id) failed";
        case ErrorCode::EncryptionFailed:     return "Encryption operation failed";
        case ErrorCode::DecryptionFailed:     return "Decryption operation failed";
        case ErrorCode::AuthenticationFailed: return "Authentication failed - wrong key or corrupted/tampered data";
        case ErrorCode::FileNotFound:         return "File not found";
        case ErrorCode::FileReadError:        return "Failed to read file";
        case ErrorCode::FileWriteError:       return "Failed to write file";
        case ErrorCode::InvalidInput:         return "Invalid input parameters";
    }
    return "Unknown error";
}

template <typename T>
class Result {
public:
    static Result<T> ok(T value)
    {
        Result<T> r;
        r.m_ok = true;
        r.m_value = std::move(value);
        return r;
    }

    static Result<T> fail(ErrorCode code, std::string detail = "")
    {
        Result<T> r;
        r.m_ok = false;
        r.m_error = code;
        r.m_detail = std::move(detail);
        return r;
    }

    bool isOk() const { return m_ok; }
    explicit operator bool() const { return m_ok; }

    const T& value() const { return m_value; }
    T& value() { return m_value; }

    ErrorCode error() const { return m_error; }
    const std::string& detail() const { return m_detail; }

    std::string errorMessage() const
    {
        std::string msg = toString(m_error);
        if (!m_detail.empty()) {
            msg += ": " + m_detail;
        }
        return msg;
    }

private:
    bool m_ok = false;
    T m_value{};
    ErrorCode m_error = ErrorCode::None;
    std::string m_detail;
};

template <>
class Result<void> {
public:
    static Result<void> ok()
    {
        Result<void> r;
        r.m_ok = true;
        return r;
    }

    static Result<void> fail(ErrorCode code, std::string detail = "")
    {
        Result<void> r;
        r.m_ok = false;
        r.m_error = code;
        r.m_detail = std::move(detail);
        return r;
    }

    bool isOk() const { return m_ok; }
    explicit operator bool() const { return m_ok; }

    ErrorCode error() const { return m_error; }
    const std::string& detail() const { return m_detail; }

    std::string errorMessage() const
    {
        std::string msg = toString(m_error);
        if (!m_detail.empty()) {
            msg += ": " + m_detail;
        }
        return msg;
    }

private:
    bool m_ok = false;
    ErrorCode m_error = ErrorCode::None;
    std::string m_detail;
};

} // namespace bytelock
