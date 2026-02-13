#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/event_bus.hpp"
#include "../core/fd.hpp"
#include "../core/reactor.hpp"
#include "../core/time_utils.hpp"

namespace irr {
struct IcmpTarget {
    std::string name;
    std::string ip;
    int interval_ms;
    int timeout_ms;
};

class IcmpProbe {
   public:
    IcmpProbe(EventBus& bus, const std::string& run_id);
    bool can_run() const {
        return can_run_;
    }
    void tick(Reactor& r, const std::vector<IcmpTarget>& targets);
    void sweep_timeouts();

   private:
    struct Attempt {
        int fd;
        uint16_t seq;
        uint64_t start_ns;
        IcmpTarget target;
    };

    EventBus& bus_;
    std::string run_id_;
    bool can_run_{false};
    Reactor* reactor_{nullptr};
    std::unordered_map<int, Attempt> inflight_;
    uint16_t next_seq_{1};

    int open_socket();
    void send_ping(Reactor& r, const IcmpTarget& t);
    void handle_recv(int fd);
    void emit_timeout(const Attempt& a);
};
}  // namespace irr
