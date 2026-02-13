#include "tcp_connect.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../core/logger.hpp"

namespace irr {
static bool set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

TcpConnectProbe::TcpConnectProbe(EventBus& bus, const std::string& run_id)
    : bus_(bus), run_id_(run_id) {}

void TcpConnectProbe::start(Reactor& r, int interval_ms, const std::vector<TcpTarget>& targets) {
    reactor_ = &r;
    targets_ = targets;
    for (const auto& t : targets_) new_attempt(t);
}

void TcpConnectProbe::stop() {
    for (auto& kv : inflight_) ::close(kv.first);
    inflight_.clear();
}

void TcpConnectProbe::new_attempt(const TcpTarget& t) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    std::string port = std::to_string(t.port);
    if (getaddrinfo(t.host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
        Event ev{run_id_, monotonic_ns(), wall_time_iso8601(), "probe.tcp.connect",
                 t.name, t.host, "unknown", t.interval_ms, t.timeout_ms, false, 0.0, "dns_failure"};
        bus_.emit(ev);
        return;
    }
    int fd = ::socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return;
    }
    set_nonblock(fd);
    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
        ::close(fd);
        freeaddrinfo(res);
        Event ev{run_id_, monotonic_ns(), wall_time_iso8601(), "probe.tcp.connect",
                 t.name, t.host, "unknown", t.interval_ms, t.timeout_ms, false, 0.0, "connect_immediate_fail"};
        bus_.emit(ev);
        return;
    }
    char ipbuf[64] = {};
    if (res->ai_family == AF_INET) {
        inet_ntop(AF_INET, &((sockaddr_in*)res->ai_addr)->sin_addr, ipbuf, sizeof(ipbuf));
    } else if (res->ai_family == AF_INET6) {
        inet_ntop(AF_INET6, &((sockaddr_in6*)res->ai_addr)->sin6_addr, ipbuf, sizeof(ipbuf));
    } else {
        std::snprintf(ipbuf, sizeof(ipbuf), "unknown");
    }
    Attempt a{fd, monotonic_ns(), t.name, ipbuf, res->ai_family == AF_INET6 ? "inet6" : "inet",
              t.interval_ms, t.timeout_ms};
    inflight_[fd] = a;
    freeaddrinfo(res);
    reactor_->add_fd(fd, EPOLLOUT | EPOLLERR, [this, fd](uint32_t ev) { handle_event(fd, ev); });
}

void TcpConnectProbe::handle_event(int fd, uint32_t events) {
    (void)events;
    auto it = inflight_.find(fd);
    if (it == inflight_.end()) return;
    int err = 0;
    socklen_t len = sizeof(err);
    ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    double ms = (monotonic_ns() - it->second.start_ns) / 1e6;
    bool ok = (err == 0);
    Event ev{run_id_, monotonic_ns(), wall_time_iso8601(), "probe.tcp.connect",
             it->second.name, it->second.ip, it->second.family,
             it->second.interval_ms, it->second.timeout_ms, ok, ms,
             ok ? "" : std::string("so_error_") + std::to_string(err)};
    bus_.emit(ev);
    reactor_->del_fd(fd);
    ::close(fd);
    inflight_.erase(it);
}
}  // namespace irr
