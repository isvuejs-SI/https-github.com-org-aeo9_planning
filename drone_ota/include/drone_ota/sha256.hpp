#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace drone_ota {

std::string to_hex(const std::uint8_t* data, std::size_t size);
std::vector<std::uint8_t> from_hex(const std::string& hex);
std::string sha256_hex(const std::uint8_t* data, std::size_t size);
std::string sha256_hex(const std::string& bytes);

}  // namespace drone_ota
