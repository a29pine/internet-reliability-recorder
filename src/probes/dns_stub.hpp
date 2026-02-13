#pragma once
#include "../core/logger.hpp"

namespace irr {
class DnsProbeStub {
   public:
    void describe() {
        log(LogLevel::INFO, "DNS probe stub (UDP+TCP fallback planned)");
    }
};
}  // namespace irr
