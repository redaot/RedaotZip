// Implementare OpenSSL - folosita DOAR pentru testele automate pe Linux.
#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <stdexcept>

namespace rz {

bool RandomBytes(uint8_t* out, size_t n) { return RAND_bytes(out, (int)n) == 1; }

bool Pbkdf2Sha1(const std::string& pw, const uint8_t* salt, size_t saltLen,
                uint32_t it, uint8_t* out, size_t outLen) {
    return PKCS5_PBKDF2_HMAC(pw.data(), (int)pw.size(), salt, (int)saltLen, (int)it, EVP_sha1(), (int)outLen, out) == 1;
}

struct HmacSha1::Impl { EVP_MAC* mac = nullptr; EVP_MAC_CTX* ctx = nullptr; };
HmacSha1::HmacSha1(const uint8_t* key, size_t keyLen) : p(new Impl) {
    p->mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    p->ctx = EVP_MAC_CTX_new(p->mac);
    OSSL_PARAM params[] = { OSSL_PARAM_construct_utf8_string("digest", (char*)"SHA1", 0), OSSL_PARAM_construct_end() };
    if (EVP_MAC_init(p->ctx, key, keyLen, params) != 1) throw std::runtime_error("hmac init");
}
HmacSha1::~HmacSha1() { EVP_MAC_CTX_free(p->ctx); EVP_MAC_free(p->mac); }
void HmacSha1::Update(const uint8_t* d, size_t n) { EVP_MAC_update(p->ctx, d, n); }
void HmacSha1::Final(uint8_t out[20]) { size_t l = 20; EVP_MAC_final(p->ctx, out, &l, 20); }

struct AesCtrWz::Impl { EVP_CIPHER_CTX* ctx = nullptr; };
AesCtrWz::AesCtrWz(const uint8_t* key, size_t keyLen) : p(new Impl) {
    p->ctx = EVP_CIPHER_CTX_new();
    const EVP_CIPHER* c = keyLen == 16 ? EVP_aes_128_ecb() : keyLen == 24 ? EVP_aes_192_ecb() : EVP_aes_256_ecb();
    EVP_EncryptInit_ex(p->ctx, c, nullptr, key, nullptr);
    EVP_CIPHER_CTX_set_padding(p->ctx, 0);
}
AesCtrWz::~AesCtrWz() { EVP_CIPHER_CTX_free(p->ctx); }
void AesCtrWz::EcbEncrypt(uint8_t* buf, size_t n) { int l = 0; EVP_EncryptUpdate(p->ctx, buf, &l, buf, (int)n); }

} // namespace rz
