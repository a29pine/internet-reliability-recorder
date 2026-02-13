#include "icmp_probe.hpp"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "../core/logger.hpp"

namespace irr {
namespace {
uint16_t csum(const uint16_t* data, size_t len) {
    uint32_t sum = 0;
    for (; len > 1; len -= 2) sum += *data++;
    if (len == 1) sum += *reinterpret_cast<const uint8_t*>(data);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}
}  // namespace

IcmpProbe::IcmpProbe(EventBus& bus, const std::string& run_id) : bus_(bus), run_id_(run_id) {
    int fd = ::socket(AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_ICMP);
    if (fd >= 0) {
        ::close(fd);
        can_run_ = true;
    }
}

int IcmpProbe::open_socket() {
    int fd = ::socket(AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_ICMP);
    if (fd < 0) return -1;
    int ttl = 64;
    ::setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    return fd;
}

void IcmpProbe::tick(Reactor& r, const std::vector<IcmpTarget>& targets) {
    if (!can_run_) return;
    reactor_ = &r;
    for (const auto& t : targets) send_ping(r, t);
    sweep_timeouts();
}

void IcmpProbe::send_ping(Reactor& r, const IcmpTarget& t) {
    int fd = open_socket();
    if (fd < 0) return;
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    if (::inet_pton(AF_INET, t.ip.c_str(), &sa.sin_addr) != 1) {
        ::close(fd);
        return;
    }
    uint16_t seq = next_seq_++;
    icmphdr hdr{};
    hdr.type = ICMP_ECHO;
    hdr.code = 0;
    hdr.un.echo.id = static_cast<uint16_t>(::getpid() & 0xFFFF);
    hdr.un.echo.sequence = seq;
    hdr.checksum = 0;
    hdr.checksum = csum(reinterpret_cast<uint16_t*>(&hdr), sizeof(hdr));
    ssize_t n = ::sendto(fd, &hdr, sizeof(hdr), 0, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    if (n < 0) {
        ::close(fd);
        return;
    }
    Attempt a{fd, seq, monotonic_ns(), t};
    inflight_[fd] = a;
    r.add_fd(fd, EPOLLIN, [this, fd](uint32_t) { handle_recv(fd); });
}

void IcmpProbe::handle_recv(int fd) {
    uint8_t buf[1500];
    sockaddr_in from{};
    socklen_t flen = sizeof(from);
    ssize_t n = ::recvfrom(fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &flen);
    auto it = inflight_.find(fd);
    if (n < static_cast<ssize_t>(sizeof(iphdr) + sizeof(icmphdr)) || it == inflight_.end()) {
        reactor_->del_fd(fd);
        ::close(fd);
        inflight_.erase(fd);
        return;
    }
    auto* ip = reinterpret_cast<iphdr*>(buf);
    auto* icmp = reinterpret_cast<icmphdr*>(buf + ip->ihl * 4);
    if (icmp->type != ICMP_ECHOREPLY || icmp->un.echo.sequence != it->second.seq) {
        return;  // keep waiting
    }
    double ms = (monotonic_ns() - it->second.start_ns) / 1e6;
    Event ev;
    ev.run_id = run_id_;
    ev.ts_monotonic_ns = monotonic_ns();
    ev.ts_wall = wall_time_iso8601();
    ev.type = "probe.icmp.rtt";
    ev.target_name = it->second.target.name;
    ev.target_ip = it->second.target.ip;
    ev.target_family = "inet";
    ev.interval_ms = it->second.target.interval_ms;
    ev.timeout_ms = it->second.target.timeout_ms;
    ev.ok = true;
    ev.metric_ms = ms;
    ev.error_category = "";
    bus_.emit(ev);
    reactor_->del_fd(fd);
    ::close(fd);
    inflight_.erase(fd);
}

void IcmpProbe::emit_timeout(const Attempt& a) {
    Event ev;
    ev.run_id = run_id_;
    ev.ts_monotonic_ns = monotonic_ns();
    ev.ts_wall = wall_time_iso8601();
    ev.type = "probe.icmp.timeout";
    ev.target_name = a.target.name;
    ev.target_ip = a.target.ip;
    ev.target_family = "inet";
    ev.interval_ms = a.target.interval_ms;
    ev.timeout_ms = a.target.timeout_ms;
    ev.ok = false;
    ev.metric_ms = (monotonic_ns() - a.start_ns) / 1e6;
    ev.error_category = "timeout";
    bus_.emit(ev);
}

void IcmpProbe::sweep_timeouts() {
    if (!reactor_) return;
    uint64_t now = monotonic_ns();
    for (auto it = inflight_.begin(); it != inflight_.end();) {
        auto& a = it->second;
        double elapsed = (now - a.start_ns) / 1e6;
        if (elapsed > a.target.timeout_ms) {
            emit_timeout(a);
            reactor_->del_fd(it->first);
            ::close(it->first);
            it = inflight_.erase(it);
        } else {
            ++it;
        }
    }
}
}  // namespace irr
