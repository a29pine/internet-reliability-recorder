#pragma once
#include "../core/event_bus.hpp"
#include "../core/reactor.hpp"

namespace irr {
class NetlinkMonitor {
   public:
    NetlinkMonitor(EventBus& bus, const std::string& run_id);
    bool start(Reactor& r);
    void stop();

   private:
    int fd_{-1};
    EventBus& bus_;
    std::string run_id_;
    void handle(uint32_t events);
};
}  // namespace irr
