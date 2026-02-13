#include "pmtu_probe.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <vector>

#include "../core/logger.hpp"

namespace irr {
PmtuProbe::PmtuProbe(EventBus& bus, const std::string& run_id) : bus_(bus), run_id_(run_id) {}

void PmtuProbe::tick(const std::vector<PmtuTarget>& targets) {
    for (const auto& t : targets) {
        int discovered = 0;
        int attempts = 0;
        int successes = 0;
        std::vector<int> sizes{1500, 1480, 1460, 1400, 1280, 1200};
        for (int sz : sizes) {
            ++attempts;
            if (probe_target(t, sz, discovered)) {
                discovered = sz;
                ++successes;
                break;
            }
        }
        std::string category;
        if (discovered == 0)
            category = "emsgsize";
        else if (successes >= 1 && attempts <= 2)
            category = "confidence_high";
        else if (successes >= 1 && attempts <= 4)
            category = "confidence_medium";
        else
            category = "confidence_low";

        Event ev;
        ev.run_id = run_id_;
        ev.ts_monotonic_ns = monotonic_ns();
        ev.ts_wall = wall_time_iso8601();
        ev.type = "probe.pmtu.result";
        ev.target_name = t.name;
        ev.target_ip = t.host;
        ev.target_family = "inet";
        ev.interval_ms = 0;
        ev.timeout_ms = 0;
        ev.ok = discovered > 0;
        ev.metric_ms = static_cast<double>(discovered);
        ev.error_category = category;
        bus_.emit(ev);
    }
}

bool PmtuProbe::probe_target(const PmtuTarget& t, int size, int& discovered) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(t.host.c_str(), std::to_string(t.port).c_str(), &hints, &res) != 0 || !res)
        return false;
    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return false;
    }
    int val = IP_PMTUDISC_DO;
    if (res->ai_family == AF_INET) {
        ::setsockopt(fd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
    } else if (res->ai_family == AF_INET6) {
#ifdef IPPROTO_IPV6
        val = IPV6_PMTUDISC_DO;
        ::setsockopt(fd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &val, sizeof(val));
#endif
    }
    std::vector<uint8_t> payload(size, 0x42);
    ssize_t rc = ::sendto(fd, payload.data(), payload.size(), 0, res->ai_addr, res->ai_addrlen);
    if (rc < 0 && errno == EMSGSIZE) {
        int mtu = 0;
        socklen_t len = sizeof(mtu);
        if (res->ai_family == AF_INET) ::getsockopt(fd, IPPROTO_IP, IP_MTU, &mtu, &len);
#ifdef IPPROTO_IPV6
        if (res->ai_family == AF_INET6) ::getsockopt(fd, IPPROTO_IPV6, IPV6_MTU, &mtu, &len);
#endif
        if (mtu > 0) discovered = mtu;
        ::close(fd);
        freeaddrinfo(res);
        return false;
    }
    ::close(fd);
    freeaddrinfo(res);
    discovered = size;
    return true;
}
}  // namespace irr
