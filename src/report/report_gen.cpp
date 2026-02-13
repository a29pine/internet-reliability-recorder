#include "report_gen.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/logger.hpp"
#include "../util/percentile.hpp"

namespace irr {
namespace {
struct ParsedEventLine {
    std::string type;
    std::string target_name;
    bool ok{false};
    bool has_ok{false};
    double metric_ms{0};
    bool has_metric{false};
};

std::string html_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

// Extract substring that is the object value of a key, handling nested braces.
bool extract_object_substr(const std::string& line, const std::string& key, std::string& out) {
    auto pos = line.find("\"" + key + "\":{");
    if (pos == std::string::npos) return false;
    pos = line.find('{', pos);
    if (pos == std::string::npos) return false;
    int depth = 0;
    for (size_t i = pos; i < line.size(); ++i) {
        if (line[i] == '{')
            depth++;
        else if (line[i] == '}')
            depth--;
        if (depth == 0) {
            out = line.substr(pos, i - pos + 1);
            return true;
        }
    }
    return false;
}

bool extract_bool(const std::string& line, const std::string& key, bool& out) {
    auto pos = line.find("\"" + key + "\":");
    if (pos == std::string::npos) return false;
    pos += key.size() + 3;
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
    if (line.compare(pos, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (line.compare(pos, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

bool extract_double(const std::string& line, const std::string& key, double& out) {
    auto pos = line.find("\"" + key + "\":");
    if (pos == std::string::npos) return false;
    pos += key.size() + 3;
    size_t end = pos;
    while (end < line.size() && (std::isdigit(static_cast<unsigned char>(line[end])) ||
                                 line[end] == '.' || line[end] == '-'))
        ++end;
    try {
        out = std::stod(line.substr(pos, end - pos));
        return true;
    } catch (...) {
        return false;
    }
}

bool extract_string(const std::string& line, const std::string& key, std::string& out) {
    auto pos = line.find("\"" + key + "\":\"");
    if (pos == std::string::npos) return false;
    size_t start = pos + key.size() + 4;
    size_t end = line.find('"', start);
    if (end == std::string::npos) return false;
    out = line.substr(start, end - start);
    return true;
}

bool parse_event_line(const std::string& line, ParsedEventLine& ev) {
    if (!extract_string(line, "type", ev.type)) return false;
    ev.has_ok = extract_bool(line, "ok", ev.ok);
    ev.has_metric = extract_double(line, "metric_ms", ev.metric_ms);
    // target nested object
    std::string target_obj;
    if (extract_object_substr(line, "target", target_obj)) {
        extract_string(target_obj, "name", ev.target_name);
    }
    return ev.has_metric;
}

std::string build_svg_polyline(const std::vector<double>& vals, double width, double height) {
    if (vals.empty()) return "";
    double maxv = 1.0;
    for (double v : vals) maxv = std::max(maxv, v);
    std::ostringstream oss;
    double step = width / (vals.size() - 1 + 1e-6);
    for (size_t i = 0; i < vals.size(); ++i) {
        double x = i * step;
        double y = height - (vals[i] / maxv) * height;
        oss << (i == 0 ? "" : " ") << x << "," << y;
    }
    return oss.str();
}
}  // namespace

bool generate_report(const std::string& bundle_in, const std::string& out_html,
                     ReportStats& stats) {
    std::ifstream in(bundle_in + "/events.jsonl");
    if (!in.is_open()) {
        log(LogLevel::ERROR, "Cannot open events.jsonl");
        return false;
    }
    std::vector<double> metrics;
    size_t failures = 0;
    size_t total = 0;
    std::string line;
    while (std::getline(in, line)) {
        ParsedEventLine parsed;
        if (!parse_event_line(line, parsed)) continue;
        if (parsed.type != "probe.tcp.connect" && parsed.type != "probe.dns.result" &&
            parsed.type != "probe.dns.timeout" && parsed.type != "probe.icmp.rtt" &&
            parsed.type != "probe.icmp.timeout")
            continue;
        ++total;
        if (parsed.has_ok && parsed.ok) {
            metrics.push_back(parsed.metric_ms);
            stats.per_target[parsed.target_name].push_back(parsed.metric_ms);
            if (parsed.type == "probe.tcp.connect") stats.timeline.push_back(parsed.metric_ms);
        } else {
            ++failures;
            stats.per_target_fail[parsed.target_name] += 1;
        }
    }
    stats.total = total;
    stats.failures = failures;
    stats.loss_pct = total == 0 ? 0.0 : (failures * 100.0 / total);
    stats.p50_ms = percentile(metrics, 50);
    stats.p95_ms = percentile(metrics, 95);
    stats.p99_ms = percentile(metrics, 99);

    std::ofstream out(out_html);
    if (!out.is_open()) return false;

    std::string poly = build_svg_polyline(stats.timeline, 600, 160);

    out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>IRR Report</title>";
    out << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    out << "<style>body{font-family:Arial;margin:24px;} "
           ".card{display:inline-block;margin:8px;padding:12px;border:1px solid "
           "#ddd;border-radius:8px;} .fail{color:#b00;} table{border-collapse:collapse;} "
           "td,th{border:1px solid #ddd;padding:6px;} svg{border:1px solid "
           "#ddd;border-radius:6px;}</style>";
    out << "</head><body>";
    out << "<h1>Internet Reliability Recorder</h1>";
    out << "<p>Summary for bundle: " << html_escape(bundle_in) << "</p>";
    out << "<div class='card'>p50: " << stats.p50_ms << " ms</div>";
    out << "<div class='card'>p95: " << stats.p95_ms << " ms</div>";
    out << "<div class='card'>p99: " << stats.p99_ms << " ms</div>";
    out << "<div class='card'>loss: " << stats.loss_pct << " %</div>";
    out << "<div class='card " << (stats.failures ? "fail" : "") << "'>failures: " << stats.failures
        << "</div>";

    out << "<h2>Per-target "
           "breakdown</h2><table><tr><th>Target</th><th>p50</th><th>p95</th><th>p99</"
           "th><th>failures</th></tr>";
    for (const auto& kv : stats.per_target) {
        double p50 = percentile(kv.second, 50);
        double p95 = percentile(kv.second, 95);
        double p99 = percentile(kv.second, 99);
        size_t fails = stats.per_target_fail[kv.first];
        out << "<tr><td>" << html_escape(kv.first) << "</td><td>" << p50 << "</td><td>" << p95
            << "</td><td>" << p99 << "</td><td>" << fails << "</td></tr>";
    }
    out << "</table>";

    out << "<h2>Timeline (TCP connect)</h2>";
    if (!poly.empty()) {
        out << "<svg width='640' height='200'><polyline fill='none' stroke='#0a74da' "
               "stroke-width='2' points='"
            << poly << "' /></svg>";
    } else {
        out << "<p>No data</p>";
    }

    out << "<h2>Findings</h2><ul>";
    if (stats.loss_pct > 5.0) out << "<li>High loss observed.</li>";
    if (stats.p99_ms > 500) out << "<li>Long tail latency spikes.</li>";
    if (stats.failures == 0 && stats.p95_ms < 150) out << "<li>Connectivity looks healthy.</li>";
    if (stats.per_target_fail.size() > 0)
        out << "<li>Per-target failures present; inspect DNS and PMTU events.</li>";
    out << "</ul>";

    out << "</body></html>";
    return true;
}
}  // namespace irr
