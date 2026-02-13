#pragma once
#include <chrono>
#include <ctime>
#include <string>

namespace irr {
inline uint64_t monotonic_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

inline std::string wall_time_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&t));
    return std::string(buf);
}
}  // namespace irr
