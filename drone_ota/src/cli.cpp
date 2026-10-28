#include "drone_ota/ed25519.hpp"
#include "drone_ota/sha256.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "usage: drone_ota_cli verify-sig|sign-message\n";
            return 2;
        }
        const std::string cmd = argv[1];
        if (cmd == "verify-sig") {
            if (argc != 5) {
                std::cerr << "usage: drone_ota_cli verify-sig <pubhex> <sighex> <file>\n";
                return 2;
            }
            const auto pub = drone_ota::from_hex(argv[2]);
            const auto sig = drone_ota::from_hex(argv[3]);
            const std::string message = read_file(argv[4]);
            const bool ok = drone_ota::verify_message(
                pub, reinterpret_cast<const std::uint8_t*>(message.data()), message.size(), sig);
            std::cout << (ok ? "VERIFY_OK\n" : "VERIFY_FAIL\n");
            return 0;
        }
        if (cmd == "sign-message") {
            if (argc != 3) {
                std::cerr << "usage: drone_ota_cli sign-message <file>\n";
                return 2;
            }
            const std::string message = read_file(argv[2]);
            const drone_ota::KeyPair pair = drone_ota::generate_keypair();
            const auto sig = drone_ota::sign_message(
                pair.priv, reinterpret_cast<const std::uint8_t*>(message.data()), message.size());
            std::cout << "PUB " << drone_ota::to_hex(pair.pub.data(), pair.pub.size()) << "\n";
            std::cout << "SIG " << drone_ota::to_hex(sig.data(), sig.size()) << "\n";
            return 0;
        }
        std::cerr << "unknown command\n";
        return 2;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 2;
    }
}
