#pragma once
#include <functional>

#include "fd.hpp"
#include "reactor.hpp"

namespace irr {
class TimerScheduler {
   public:
    TimerScheduler();
    ~TimerScheduler();
    bool start(Reactor& r, int interval_ms, const std::function<void()>& cb);
    void stop();

   private:
    Fd tfd_;
    std::function<void()> cb_;
};
}  // namespace irr
