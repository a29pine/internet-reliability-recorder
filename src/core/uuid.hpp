#pragma once
#include <random>
#include <string>

namespace irr {
inline std::string uuid4() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    uint64_t a = rng(), b = rng();
    std::string out(36, '0');
    int idx = 0;
    auto write = [&](uint64_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            out[idx++] = hex[(v >> 60) & 0xF];
            v <<= 4;
        }
    };
    write(a, 8);
    out[idx++] = '-';
    write(a << 32, 4);
    out[idx++] = '-';
    write(b, 4);
    out[idx++] = '-';
    write(b << 16, 4);
    out[idx++] = '-';
    write(b << 32, 12);
    return out;
}
}  // namespace irr
