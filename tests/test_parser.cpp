#include <string>
#include <fstream>
#include <filesystem>
#include "../src/report/report_gen.hpp"

int main() {
    std::string line = "{\"type\":\"probe.tcp.connect\",\"target\":{\"name\":\"t\"},\"result\":{\"ok\":true,\"metric_ms\":12.5}}";
    // use internal parser by calling generate_report on temp file
    std::string dir = "/tmp/irr_parser_test";
    std::filesystem::create_directories(dir);
    std::ofstream ev(dir + "/events.jsonl");
    ev << line << "\n";
    ev.close();
    irr::ReportStats stats;
    bool ok = irr::generate_report(dir, dir + "/report.html", stats);
    if (!ok) return 1;
    if (stats.total != 1) return 2;
    if (stats.per_target["t"].size() != 1) return 3;
    if (stats.p50_ms != 12.5) return 4;
    return 0;
}
