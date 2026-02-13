#include "reactor.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include "logger.hpp"

namespace irr {
Reactor::Reactor() {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) log(LogLevel::ERROR, "epoll_create1 failed");
}

Reactor::~Reactor() {
    if (epoll_fd_ >= 0) ::close(epoll_fd_);
}

bool Reactor::add_fd(int fd, uint32_t events, const FdHandler& cb) {
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) return false;
    handlers_[fd] = cb;
    return true;
}

bool Reactor::mod_fd(int fd, uint32_t events) {
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

void Reactor::del_fd(int fd) {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    handlers_.erase(fd);
}

void Reactor::loop_once(int timeout_ms) {
    struct epoll_event evs[32];
    int n = ::epoll_wait(epoll_fd_, evs, 32, timeout_ms);
    if (n < 0) return;
    for (int i = 0; i < n; ++i) {
        int fd = evs[i].data.fd;
        auto it = handlers_.find(fd);
        if (it != handlers_.end()) {
            it->second(evs[i].events);
        }
    }
}
}  // namespace irr
