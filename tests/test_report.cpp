#include <fstream>
#include <string>
#include <filesystem>
#include "../src/report/report_gen.hpp"

int main() {
    std::string dir = "/tmp/irr_report_test";
    std::filesystem::create_directories(dir);
    std::ofstream ev(dir + "/events.jsonl");
    ev << "{\"run_id\":\"r\",\"ts_monotonic_ns\":1,\"ts_wall\":\"2024-01-01T00:00:00Z\",\"type\":\"probe.tcp.connect\",\"target\":{\"name\":\"t1\"},\"probe\":{\"interval_ms\":1000,\"timeout_ms\":2000},\"result\":{\"ok\":true,\"metric_ms\":10}}\n";
    ev << "{\"run_id\":\"r\",\"ts_monotonic_ns\":2,\"ts_wall\":\"2024-01-01T00:00:01Z\",\"type\":\"probe.tcp.connect\",\"target\":{\"name\":\"t1\"},\"probe\":{\"interval_ms\":1000,\"timeout_ms\":2000},\"result\":{\"ok\":false,\"metric_ms\":0}}\n";
    ev << "{\"run_id\":\"r\",\"ts_monotonic_ns\":3,\"ts_wall\":\"2024-01-01T00:00:02Z\",\"type\":\"probe.dns.result\",\"target\":{\"name\":\"dns\"},\"probe\":{\"interval_ms\":1000,\"timeout_ms\":2000},\"result\":{\"ok\":true,\"metric_ms\":20}}\n";
    ev << "{\"run_id\":\"r\",\"ts_monotonic_ns\":4,\"ts_wall\":\"2024-01-01T00:00:03Z\",\"type\":\"probe.tcp.connect\",\"target\":{\"name\":\"<img src=x onerror=alert(1)>\"},\"probe\":{\"interval_ms\":1000,\"timeout_ms\":2000},\"result\":{\"ok\":true,\"metric_ms\":30}}\n";
    ev.close();
    irr::ReportStats stats;
    bool ok = irr::generate_report(dir, dir + "/report.html", stats);
    if (!ok) return 1;
    if (stats.total != 4) return 2;
    if (stats.failures != 1) return 3;
    if (stats.per_target["t1"].size() != 1) return 4;
    std::ifstream html(dir + "/report.html");
    std::string contents((std::istreambuf_iterator<char>(html)), std::istreambuf_iterator<char>());
    if (contents.find("<script") != std::string::npos) return 5;
    if (contents.find("&lt;img src=x onerror=alert(1)&gt;") == std::string::npos) return 6;
    return 0;
}
