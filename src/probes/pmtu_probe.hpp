#pragma once
#include <string>
#include <vector>

#include "../core/event_bus.hpp"
#include "../core/time_utils.hpp"

namespace irr {
struct PmtuTarget {
    std::string name;
    std::string host;
    int port;
};

class PmtuProbe {
   public:
    PmtuProbe(EventBus& bus, const std::string& run_id);
    void tick(const std::vector<PmtuTarget>& targets);

   private:
    EventBus& bus_;
    std::string run_id_;
    bool probe_target(const PmtuTarget& t, int size, int& discovered);
};
}  // namespace irr
