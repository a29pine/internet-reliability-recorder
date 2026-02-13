#pragma once
#include "../core/logger.hpp"

namespace irr {
class IcmpProbeStub {
   public:
    void describe() { log(LogLevel::INFO, "ICMP probe stub (needs CAP_NET_RAW)"); }
};
}  // namespace irr
