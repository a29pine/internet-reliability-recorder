#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace irr {
struct Event {
    std::string run_id;
    uint64_t ts_monotonic_ns{};
    std::string ts_wall;
    std::string type;
    std::string target_name;
    std::string target_ip;
    std::string target_family;
    int interval_ms{};
    int timeout_ms{};
    bool ok{};
    double metric_ms{};
    std::string error_category;
};

class EventSink {
   public:
    virtual ~EventSink() = default;
    virtual void on_event(const Event& ev) = 0;
};

class EventBus {
   public:
    void add_sink(EventSink* sink) {
        sinks_.push_back(sink);
    }
    void emit(const Event& ev) {
        for (auto* s : sinks_) s->on_event(ev);
    }

   private:
    std::vector<EventSink*> sinks_;
};
}  // namespace irr
