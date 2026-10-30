// Implementare cu Windows CNG (bcrypt.dll) - fara biblioteci externe.
#include "crypto.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <vector>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

namespace rz {

static void Check(NTSTATUS s, const char* what) {
    if (!NT_SUCCESS(s)) throw std::runtime_error(std::string("Eroare criptografica Windows: ") + what);
}

bool RandomBytes(uint8_t* out, size_t n) {
    return NT_SUCCESS(BCryptGenRandom(nullptr, out, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool Pbkdf2Sha1(const std::string& password, const uint8_t* salt, size_t saltLen,
                uint32_t iterations, uint8_t* out, size_t outLen) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
        return false;
    NTSTATUS s = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)password.data(), (ULONG)password.size(),
                                       (PUCHAR)salt, (ULONG)saltLen, iterations, out, (ULONG)outLen, 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    return NT_SUCCESS(s);
}

struct HmacSha1::Impl {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
};

HmacSha1::HmacSha1(const uint8_t* key, size_t keyLen) : p(new Impl) {
    Check(BCryptOpenAlgorithmProvider(&p->alg, BCRYPT_SHA1_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG), "HMAC open");
    Check(BCryptCreateHash(p->alg, &p->h, nullptr, 0, (PUCHAR)key, (ULONG)keyLen, 0), "HMAC create");
}
HmacSha1::~HmacSha1() {
    if (p->h) BCryptDestroyHash(p->h);
    if (p->alg) BCryptCloseAlgorithmProvider(p->alg, 0);
}
void HmacSha1::Update(const uint8_t* data, size_t n) {
    while (n) {
        ULONG chunk = n > 0x40000000 ? 0x40000000 : (ULONG)n;
        Check(BCryptHashData(p->h, (PUCHAR)data, chunk, 0), "HMAC data");
        data += chunk; n -= chunk;
    }
}
void HmacSha1::Final(uint8_t out[20]) { Check(BCryptFinishHash(p->h, out, 20, 0), "HMAC final"); }

struct AesCtrWz::Impl {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
};

AesCtrWz::AesCtrWz(const uint8_t* key, size_t keyLen) : p(new Impl) {
    Check(BCryptOpenAlgorithmProvider(&p->alg, BCRYPT_AES_ALGORITHM, nullptr, 0), "AES open");
    Check(BCryptSetProperty(p->alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB,
                            (ULONG)sizeof(BCRYPT_CHAIN_MODE_ECB), 0), "AES ECB");
    Check(BCryptGenerateSymmetricKey(p->alg, &p->key, nullptr, 0, (PUCHAR)key, (ULONG)keyLen, 0), "AES key");
}
AesCtrWz::~AesCtrWz() {
    if (p->key) BCryptDestroyKey(p->key);
    if (p->alg) BCryptCloseAlgorithmProvider(p->alg, 0);
}
void AesCtrWz::EcbEncrypt(uint8_t* buf, size_t n) {
    ULONG done = 0;
    Check(BCryptEncrypt(p->key, buf, (ULONG)n, nullptr, nullptr, 0, buf, (ULONG)n, &done, 0), "AES encrypt");
}

} // namespace rz
