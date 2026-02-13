#include "netlink_monitor.hpp"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

#include "../core/logger.hpp"
#include "../core/time_utils.hpp"

namespace irr {
NetlinkMonitor::NetlinkMonitor(EventBus& bus, const std::string& run_id)
    : bus_(bus), run_id_(run_id) {}

bool NetlinkMonitor::start(Reactor& r) {
    fd_ = ::socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_ROUTE);
    if (fd_ < 0) {
        log(LogLevel::WARN, "netlink socket unavailable; skipping route monitoring");
        return false;
    }
    sockaddr_nl addr{};
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log(LogLevel::WARN, "netlink bind failed");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    r.add_fd(fd_, EPOLLIN, [this](uint32_t ev) { handle(ev); });
    return true;
}

void NetlinkMonitor::stop() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void NetlinkMonitor::handle(uint32_t) {
    char buf[4096];
    ssize_t len = ::recv(fd_, buf, sizeof(buf), 0);
    if (len <= 0) return;
    for (nlmsghdr* nh = reinterpret_cast<nlmsghdr*>(buf); NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len)) {
        Event ev;
        ev.run_id = run_id_;
        ev.ts_monotonic_ns = monotonic_ns();
        ev.ts_wall = wall_time_iso8601();
        ev.target_name = "host";
        ev.target_ip = "localhost";
        ev.target_family = "netlink";
        ev.interval_ms = 0;
        ev.timeout_ms = 0;
        ev.ok = true;
        ev.metric_ms = 0.0;
        if (nh->nlmsg_type == RTM_NEWLINK || nh->nlmsg_type == RTM_DELLINK) {
            ev.type = "sys.netlink.link_change";
            ev.error_category = nh->nlmsg_type == RTM_NEWLINK ? "link_up" : "link_down";
        } else if (nh->nlmsg_type == RTM_NEWROUTE || nh->nlmsg_type == RTM_DELROUTE) {
            ev.type = "sys.netlink.route_change";
            ev.error_category = nh->nlmsg_type == RTM_NEWROUTE ? "route_add" : "route_del";
        } else {
            continue;
        }
        bus_.emit(ev);
    }
}
}  // namespace irr
