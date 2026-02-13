#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/event_bus.hpp"
#include "../core/reactor.hpp"
#include "../core/time_utils.hpp"

namespace irr {
struct DnsTarget {
    std::string name;
    std::string qname;
    int interval_ms;
    int timeout_ms;
};

class DnsProbe {
   public:
    DnsProbe(EventBus& bus, const std::string& run_id);
    void set_resolver(const std::string& ip, int port = 53);
    void tick(Reactor& r, const std::vector<DnsTarget>& targets);
    void sweep_timeouts();

   private:
    struct Attempt {
        int fd;
        uint16_t id;
        uint64_t start_ns;
        std::string target_name;
        std::string qname;
        int timeout_ms;
        bool tcp_fallback_attempted{false};
    };

    EventBus& bus_;
    std::string run_id_;
    Reactor* reactor_{nullptr};
    std::string resolver_ip_ = "1.1.1.1";
    int resolver_port_ = 53;
    std::unordered_map<int, Attempt> inflight_;
    uint16_t next_id_{1};

    void send_udp_query(Reactor& r, const DnsTarget& t);
    void handle_response(int fd);
    void emit_event(const Attempt& a, bool ok, double ms, const std::string& category,
                    int rcode = -1);
    bool tcp_fallback(const DnsTarget& t, Attempt& a);
};
}  // namespace irr
