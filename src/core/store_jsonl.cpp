#include "store_jsonl.hpp"
#include <iomanip>
#include <sstream>
#include "logger.hpp"

namespace irr {
JsonlStore::JsonlStore(const std::string& path) : out_(path, std::ios::app) {
    is_open_ = out_.is_open();
    if (!is_open_) {
        log(LogLevel::ERROR, "JsonlStore failed to open output file: " + path);
    }
}
JsonlStore::~JsonlStore() {
    if (is_open_) out_.flush();
}

std::string JsonlStore::escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

void JsonlStore::write_json(const Event& ev) {
    if (!is_open_) return;
    out_ << "{";
    out_ << "\"run_id\":\"" << escape(ev.run_id) << "\",";
    out_ << "\"ts_monotonic_ns\":" << ev.ts_monotonic_ns << ",";
    out_ << "\"ts_wall\":\"" << ev.ts_wall << "\",";
    out_ << "\"type\":\"" << escape(ev.type) << "\",";
    out_ << "\"target\":{\"name\":\"" << escape(ev.target_name) << "\",";
    out_ << "\"ip\":\"" << escape(ev.target_ip) << "\",";
    out_ << "\"family\":\"" << escape(ev.target_family) << "\"},";
    out_ << "\"probe\":{\"interval_ms\":" << ev.interval_ms << ",\"timeout_ms\":" << ev.timeout_ms << "},";
    out_ << "\"result\":{\"ok\":" << (ev.ok ? "true" : "false") << ",\"metric_ms\":" << ev.metric_ms
         << ",\"error_category\":\"" << escape(ev.error_category) << "\"}";
    out_ << "}\n";
}

void JsonlStore::on_event(const Event& ev) { write_json(ev); }
}  // namespace irr
