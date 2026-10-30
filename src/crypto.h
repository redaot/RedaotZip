#pragma once
// Primitive criptografice folosite de formatul WinZip AES (AE-1/AE-2).
// Pe Windows: Windows CNG (bcrypt.dll). Pentru testele pe Linux: OpenSSL.
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

namespace rz {

bool RandomBytes(uint8_t* out, size_t n);

// PBKDF2-HMAC-SHA1 (impus de specificatia WinZip AES, 1000 iteratii)
bool Pbkdf2Sha1(const std::string& password, const uint8_t* salt, size_t saltLen,
                uint32_t iterations, uint8_t* out, size_t outLen);

class HmacSha1 {
public:
    HmacSha1(const uint8_t* key, size_t keyLen);
    ~HmacSha1();
    void Update(const uint8_t* data, size_t n);
    void Final(uint8_t out[20]);
private:
    struct Impl; std::unique_ptr<Impl> p;
};

// AES in mod CTR, varianta WinZip: contor little-endian pe 128 biti, porneste de la 1.
class AesCtrWz {
public:
    AesCtrWz(const uint8_t* key, size_t keyLen); // 16/24/32 bytes
    ~AesCtrWz();
    void Process(uint8_t* data, size_t n);        // criptare == decriptare
private:
    struct Impl; std::unique_ptr<Impl> p;
    uint8_t counter[16] = {0};
    uint8_t ks[16 * 4096];
    size_t ksPos = 0, ksLen = 0;
    void Refill();
    void EcbEncrypt(uint8_t* buf, size_t n); // in-place
};

} // namespace rz
