#include "crypto.h"
#include <cstring>

namespace rz {

static void IncCounterLE(uint8_t c[16]) {
    for (int i = 0; i < 16; ++i) { if (++c[i] != 0) break; }
}

void AesCtrWz::Refill() {
    const size_t blocks = sizeof(ks) / 16;
    for (size_t b = 0; b < blocks; ++b) {
        IncCounterLE(counter);            // primul bloc foloseste contorul 1
        memcpy(ks + b * 16, counter, 16);
    }
    EcbEncrypt(ks, sizeof(ks));
    ksPos = 0; ksLen = sizeof(ks);
}

void AesCtrWz::Process(uint8_t* data, size_t n) {
    while (n) {
        if (ksPos == ksLen) Refill();
        size_t take = ksLen - ksPos; if (take > n) take = n;
        for (size_t i = 0; i < take; ++i) data[i] ^= ks[ksPos + i];
        data += take; n -= take; ksPos += take;
    }
}

} // namespace rz
