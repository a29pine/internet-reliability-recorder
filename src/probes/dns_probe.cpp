#include "dns_probe.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <random>
#include "../core/logger.hpp"

namespace irr {
namespace {
uint16_t make_id() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    return static_cast<uint16_t>(rng());
}

std::vector<uint8_t> build_query(uint16_t id, const std::string& qname) {
    std::vector<uint8_t> buf;
    buf.resize(12);  // header
    buf[0] = id >> 8; buf[1] = id & 0xff;
    buf[2] = 0x01;   // recursion desired
    buf[3] = 0x00;
    buf[4] = 0x00; buf[5] = 0x01;  // QDCOUNT=1
    buf[6] = buf[7] = buf[8] = buf[9] = buf[10] = buf[11] = 0;
    // qname labels
    size_t pos = 12;
    size_t start = 0;
    while (start < qname.size()) {
        size_t dot = qname.find('.', start);
        if (dot == std::string::npos) dot = qname.size();
        size_t len = dot - start;
        buf.push_back(static_cast<uint8_t>(len));
        for (size_t i = 0; i < len; ++i) buf.push_back(static_cast<uint8_t>(qname[start + i]));
        start = dot + 1;
    }
    buf.push_back(0);  // end of name
    buf.push_back(0); buf.push_back(1);  // QTYPE A
    buf.push_back(0); buf.push_back(1);  // QCLASS IN
    return buf;
}

int rcode_from_response(const uint8_t* data, size_t len) {
    if (len < 12) return -1;
    return data[3] & 0x0F;
}
}

DnsProbe::DnsProbe(EventBus& bus, const std::string& run_id) : bus_(bus), run_id_(run_id) {}

void DnsProbe::set_resolver(const std::string& ip, int port) {
    resolver_ip_ = ip;
    resolver_port_ = port;
}

void DnsProbe::tick(Reactor& r, const std::vector<DnsTarget>& targets) {
    reactor_ = &r;
    for (const auto& t : targets) send_udp_query(r, t);
    sweep_timeouts();
}

void DnsProbe::send_udp_query(Reactor& r, const DnsTarget& t) {
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return;
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(resolver_port_);
    if (::inet_pton(AF_INET, resolver_ip_.c_str(), &sa.sin_addr) != 1) {
        ::close(fd);
        return;
    }
    uint16_t id = make_id();
    auto pkt = build_query(id, t.qname);
    ssize_t n = ::sendto(fd, pkt.data(), pkt.size(), 0, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    if (n < 0) {
        ::close(fd);
        emit_event({fd, id, monotonic_ns(), t.name, t.qname, t.timeout_ms}, false, 0.0, "send_fail");
        return;
    }
    Attempt a{fd, id, monotonic_ns(), t.name, t.qname, t.timeout_ms};
    inflight_[fd] = a;
    r.add_fd(fd, EPOLLIN, [this, fd](uint32_t) { handle_response(fd); });
}

void DnsProbe::handle_response(int fd) {
    uint8_t buf[1500];
    sockaddr_in from{};
    socklen_t flen = sizeof(from);
    ssize_t n = ::recvfrom(fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &flen);
    auto it = inflight_.find(fd);
    if (n <= 0 || it == inflight_.end()) {
        reactor_->del_fd(fd);
        ::close(fd);
        inflight_.erase(fd);
        return;
    }
    int rcode = rcode_from_response(buf, static_cast<size_t>(n));
    double ms = (monotonic_ns() - it->second.start_ns) / 1e6;
    bool ok = (rcode == 0);
    emit_event(it->second, ok, ms, ok ? "" : "dns_rcode", rcode);
    reactor_->del_fd(fd);
    ::close(fd);
    inflight_.erase(fd);
}

void DnsProbe::sweep_timeouts() {
    uint64_t now = monotonic_ns();
    for (auto it = inflight_.begin(); it != inflight_.end();) {
        auto& a = it->second;
        double elapsed_ms = (now - a.start_ns) / 1e6;
        if (elapsed_ms > a.timeout_ms) {
            bool fallback_ok = false;
            if (!a.tcp_fallback_attempted) {
                a.tcp_fallback_attempted = true;
                // attempt TCP fallback synchronously within timeout window
                fallback_ok = tcp_fallback({a.target_name, a.qname, a.timeout_ms, a.timeout_ms}, a);
            }
            emit_event(a, fallback_ok, elapsed_ms, fallback_ok ? "tcp_fallback_success" : "timeout");
            reactor_->del_fd(it->first);
            ::close(it->first);
            it = inflight_.erase(it);
        } else {
            ++it;
        }
    }
}

bool DnsProbe::tcp_fallback(const DnsTarget& t, Attempt& a) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(resolver_port_);
    if (::inet_pton(AF_INET, resolver_ip_.c_str(), &sa.sin_addr) != 1) {
        ::close(fd);
        return false;
    }
    // set non-blocking connect with timeout using select
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    if (rc < 0 && errno != EINPROGRESS) {
        ::close(fd);
        return false;
    }
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    timeval tv{};
    tv.tv_sec = t.timeout_ms / 1000;
    tv.tv_usec = (t.timeout_ms % 1000) * 1000;
    rc = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
    if (rc <= 0) {
        ::close(fd);
        return false;
    }
    // build TCP DNS query with length prefix
    auto pkt = build_query(a.id, t.qname);
    uint16_t len = htons(static_cast<uint16_t>(pkt.size()));
    std::vector<uint8_t> framed;
    framed.resize(2);
    std::memcpy(framed.data(), &len, 2);
    framed.insert(framed.end(), pkt.begin(), pkt.end());
    if (::send(fd, framed.data(), framed.size(), 0) < 0) {
        ::close(fd);
        return false;
    }
    uint8_t rbuf[2048];
    rc = ::recv(fd, rbuf, sizeof(rbuf), 0);
    ::close(fd);
    if (rc <= 0) return false;
    int rcode = rcode_from_response(rbuf + 2, static_cast<size_t>(rc - 2));
    return rcode == 0;
}

void DnsProbe::emit_event(const Attempt& a, bool ok, double ms, const std::string& category, int rcode) {
    Event ev;
    ev.run_id = run_id_;
    ev.ts_monotonic_ns = monotonic_ns();
    ev.ts_wall = wall_time_iso8601();
    ev.type = ok ? "probe.dns.result" : "probe.dns.timeout";
    ev.target_name = a.target_name;
    ev.target_ip = resolver_ip_;
    ev.target_family = "inet";
    ev.interval_ms = 0;
    ev.timeout_ms = a.timeout_ms;
    ev.ok = ok;
    ev.metric_ms = ms;
    if (rcode >= 0) ev.error_category = ok ? "" : (category + "_rcode_" + std::to_string(rcode));
    else ev.error_category = category;
    bus_.emit(ev);
}
}  // namespace irr
