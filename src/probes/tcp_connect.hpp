#pragma once
#include <netinet/in.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "../core/event_bus.hpp"
#include "../core/reactor.hpp"
#include "../core/time_utils.hpp"

namespace irr {
struct TcpTarget {
    std::string name;
    std::string host;
    int port;
    int interval_ms;
    int timeout_ms;
};

class TcpConnectProbe {
   public:
    TcpConnectProbe(EventBus& bus, const std::string& run_id);
    void start(Reactor& r, int interval_ms, const std::vector<TcpTarget>& targets);
    void stop();

   private:
    struct Attempt {
        int fd;
        uint64_t start_ns;
        std::string name;
        std::string ip;
        std::string family;
        int interval_ms;
        int timeout_ms;
    };
    EventBus& bus_;
    std::string run_id_;
    Reactor* reactor_{nullptr};
    std::vector<TcpTarget> targets_;
    std::unordered_map<int, Attempt> inflight_;
    void new_attempt(const TcpTarget& t);
    void handle_event(int fd, uint32_t events);
};
}  // namespace irr
