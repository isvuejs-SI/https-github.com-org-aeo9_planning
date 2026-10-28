#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace drone_ota {

struct KeyPair {
    std::vector<std::uint8_t> pub;
    std::vector<std::uint8_t> priv;
};

KeyPair generate_keypair();
std::vector<std::uint8_t> sign_message(const std::vector<std::uint8_t>& priv, const std::uint8_t* data, std::size_t size);
bool verify_message(const std::vector<std::uint8_t>& pub, const std::uint8_t* data, std::size_t size,
                    const std::vector<std::uint8_t>& sig);

}  // namespace drone_ota
