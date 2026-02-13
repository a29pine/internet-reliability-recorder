#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace irr {
struct ReportStats {
    double p50_ms{0}, p95_ms{0}, p99_ms{0};
    double loss_pct{0};
    size_t total{0};
    size_t failures{0};
    std::unordered_map<std::string, std::vector<double>> per_target;
    std::unordered_map<std::string, size_t> per_target_fail;
    std::vector<double> timeline;
};

bool generate_report(const std::string& bundle_in, const std::string& out_html, ReportStats& stats);
}  // namespace irr
