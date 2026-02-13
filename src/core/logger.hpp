#pragma once
#include <chrono>
#include <cstdio>
#include <string>

namespace irr {
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

inline const char* level_name(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?";
}

inline void log(LogLevel lvl, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&t));
    std::fprintf(stderr, "[%s] %s: %s\n", buf, level_name(lvl), msg.c_str());
}
}  // namespace irr
