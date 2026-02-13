#pragma once
#include <fstream>
#include <string>
#include "event_bus.hpp"

namespace irr {
class JsonlStore : public EventSink {
   public:
    explicit JsonlStore(const std::string& path);
    ~JsonlStore();
    void on_event(const Event& ev) override;

   private:
    bool is_open_{false};
    std::ofstream out_;
    void write_json(const Event& ev);
    static std::string escape(const std::string& s);
};
}  // namespace irr
