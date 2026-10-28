#include "drone_ota/ed25519.hpp"

#include <openssl/evp.h>

#include <stdexcept>

namespace drone_ota {
namespace {

struct PKey {
    EVP_PKEY* key = nullptr;
    explicit PKey(EVP_PKEY* value) : key(value) {}
    ~PKey() { EVP_PKEY_free(key); }
    PKey(const PKey&) = delete;
    PKey& operator=(const PKey&) = delete;
};

struct Md {
    EVP_MD_CTX* ctx = nullptr;
    Md() : ctx(EVP_MD_CTX_new()) {}
    ~Md() { EVP_MD_CTX_free(ctx); }
    Md(const Md&) = delete;
    Md& operator=(const Md&) = delete;
};

}  // namespace

KeyPair generate_keypair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (ctx == nullptr || EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("ed25519 keygen init failed");
    }
    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(ctx, &raw) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("ed25519 keygen failed");
    }
    EVP_PKEY_CTX_free(ctx);
    PKey pkey(raw);
    KeyPair pair;
    pair.pub.resize(32);
    pair.priv.resize(32);
    std::size_t n = 32;
    if (EVP_PKEY_get_raw_public_key(pkey.key, pair.pub.data(), &n) != 1 || n != 32) {
        throw std::runtime_error("ed25519 public key export failed");
    }
    n = 32;
    if (EVP_PKEY_get_raw_private_key(pkey.key, pair.priv.data(), &n) != 1 || n != 32) {
        throw std::runtime_error("ed25519 private key export failed");
    }
    return pair;
}

std::vector<std::uint8_t> sign_message(const std::vector<std::uint8_t>& priv, const std::uint8_t* data,
                                       std::size_t size) {
    if (priv.size() != 32) {
        throw std::runtime_error("ed25519 private key must be 32 bytes");
    }
    PKey pkey(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, priv.data(), priv.size()));
    if (pkey.key == nullptr) {
        throw std::runtime_error("ed25519 private key rejected");
    }
    Md md;
    if (md.ctx == nullptr || EVP_DigestSignInit(md.ctx, nullptr, nullptr, nullptr, pkey.key) != 1) {
        throw std::runtime_error("ed25519 sign init failed");
    }
    std::size_t sig_len = 64;
    std::vector<std::uint8_t> sig(64);
    if (EVP_DigestSign(md.ctx, sig.data(), &sig_len, data, size) != 1 || sig_len != 64) {
        throw std::runtime_error("ed25519 sign failed");
    }
    return sig;
}

bool verify_message(const std::vector<std::uint8_t>& pub, const std::uint8_t* data, std::size_t size,
                    const std::vector<std::uint8_t>& sig) {
    if (pub.size() != 32 || sig.size() != 64) {
        return false;
    }
    PKey pkey(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub.data(), pub.size()));
    if (pkey.key == nullptr) {
        return false;
    }
    Md md;
    if (md.ctx == nullptr || EVP_DigestVerifyInit(md.ctx, nullptr, nullptr, nullptr, pkey.key) != 1) {
        return false;
    }
    return EVP_DigestVerify(md.ctx, sig.data(), sig.size(), data, size) == 1;
}

}  // namespace drone_ota
