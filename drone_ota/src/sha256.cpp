#include "drone_ota/sha256.hpp"

#include <openssl/evp.h>

#include <stdexcept>

namespace drone_ota {

std::string to_hex(const std::uint8_t* data, std::size_t size) {
    static const char* digits = "0123456789abcdef";
    std::string out(size * 2, '0');
    for (std::size_t i = 0; i < size; ++i) {
        out[2 * i] = digits[data[i] >> 4];
        out[2 * i + 1] = digits[data[i] & 0x0f];
    }
    return out;
}

std::vector<std::uint8_t> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("odd hex length");
    }
    std::vector<std::uint8_t> out(hex.size() / 2);
    auto nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return -1;
    };
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = nibble(hex[2 * i]);
        const int lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            throw std::runtime_error("bad hex");
        }
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return out;
}

std::string sha256_hex(const std::uint8_t* data, std::size_t size) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(data, size, hash, &length, EVP_sha256(), nullptr) != 1 || length != 32) {
        throw std::runtime_error("sha256 failed");
    }
    return to_hex(hash, length);
}

std::string sha256_hex(const std::string& bytes) {
    return sha256_hex(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace drone_ota
