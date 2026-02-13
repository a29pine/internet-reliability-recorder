#include "scheduler_timerfd.hpp"
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <cstring>
#include "logger.hpp"

namespace irr {
TimerScheduler::TimerScheduler() = default;
TimerScheduler::~TimerScheduler() { stop(); }

bool TimerScheduler::start(Reactor& r, int interval_ms, const std::function<void()>& cb) {
    cb_ = cb;
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        log(LogLevel::ERROR, "timerfd_create failed");
        return false;
    }
    tfd_.reset(fd);
    itimerspec its{};
    its.it_interval.tv_sec = interval_ms / 1000;
    its.it_interval.tv_nsec = (interval_ms % 1000) * 1000000;
    its.it_value = its.it_interval;
    if (::timerfd_settime(fd, 0, &its, nullptr) < 0) {
        log(LogLevel::ERROR, "timerfd_settime failed");
        return false;
    }
    r.add_fd(fd, EPOLLIN, [this](uint32_t) {
        uint64_t expirations;
        (void)::read(tfd_.get(), &expirations, sizeof(expirations));
        if (cb_) cb_();
    });
    return true;
}

void TimerScheduler::stop() {
    if (tfd_) tfd_.reset();
}
}  // namespace irr
