#include <netinet/in.h>
#include <sys/socket.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "core/event_bus.hpp"
#include "core/logger.hpp"
#include "core/reactor.hpp"
#include "core/scheduler_timerfd.hpp"
#include "core/store_jsonl.hpp"
#include "core/time_utils.hpp"
#include "core/uuid.hpp"
#include "probes/dns_probe.hpp"
#include "probes/icmp_probe.hpp"
#include "probes/netlink_monitor.hpp"
#include "probes/pmtu_probe.hpp"
#include "probes/tcp_connect.hpp"
#include "report/report_gen.hpp"

using namespace irr;

static void write_manifest(const std::string& path, const std::string& run_id, int duration_s,
                           const std::string& profile, const std::vector<TcpTarget>& targets,
                           const std::vector<DnsTarget>& dns_targets,
                           const std::vector<PmtuTarget>& pmtu_targets,
                           const std::vector<IcmpTarget>& icmp_targets, int interval_ms) {
    std::ofstream out(path + "/run.json");
    out << "{\n";
    out << "  \"run_id\": \"" << run_id << "\",\n";
    out << "  \"started_at\": \"" << wall_time_iso8601() << "\",\n";
    out << "  \"duration_s\": " << duration_s << ",\n";
    out << "  \"profile\": \"" << profile << "\",\n";
    out << "  \"interval_ms\": " << interval_ms << ",\n";
    out << "  \"targets\": [\n";
    for (size_t i = 0; i < targets.size(); ++i) {
        out << "    {\"name\":\"" << targets[i].name << "\",\"host\":\"" << targets[i].host
            << "\",\"port\":" << targets[i].port << "}";
        if (i + 1 < targets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "  ,\"dns_targets\": [\n";
    for (size_t i = 0; i < dns_targets.size(); ++i) {
        out << "    {\"name\":\"" << dns_targets[i].name << "\",\"qname\":\""
            << dns_targets[i].qname << "\",\"interval_ms\":" << dns_targets[i].interval_ms << "}";
        if (i + 1 < dns_targets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "  ,\"pmtu_targets\": [\n";
    for (size_t i = 0; i < pmtu_targets.size(); ++i) {
        out << "    {\"name\":\"" << pmtu_targets[i].name << "\",\"host\":\""
            << pmtu_targets[i].host << "\",\"port\":" << pmtu_targets[i].port << "}";
        if (i + 1 < pmtu_targets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "  ,\"icmp_targets\": [\n";
    for (size_t i = 0; i < icmp_targets.size(); ++i) {
        out << "    {\"name\":\"" << icmp_targets[i].name << "\",\"ip\":\"" << icmp_targets[i].ip
            << "\",\"interval_ms\":" << icmp_targets[i].interval_ms << "}";
        if (i + 1 < icmp_targets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

static int cmd_doctor() {
    std::cout << "Doctor checks:\n";
    if (std::filesystem::exists("/etc/resolv.conf"))
        std::cout << " - resolv.conf: ok\n";
    else
        std::cout << " - resolv.conf missing\n";
    int fd = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
        std::cout << " - ICMP raw: unavailable (need CAP_NET_RAW or root)\n";
    else {
        ::close(fd);
        std::cout << " - ICMP raw: available\n";
    }
    std::cout << " - For CAP_NET_RAW: try setcap cap_net_raw+ep ./irr\n";
    return 0;
}

static std::vector<TcpTarget> default_targets(const std::string& profile) {
    if (profile == "home") {
        return {{"cloudflare", "1.1.1.1", 443, 1000, 2000}, {"google", "8.8.8.8", 443, 1000, 2000}};
    }
    return {{"quad9", "9.9.9.9", 443, 1000, 2000}};
}

static std::vector<DnsTarget> default_dns_targets(int interval_ms) {
    return {{"root-dns", "example.com", interval_ms, 2000}};
}

static std::vector<PmtuTarget> default_pmtu_targets(const std::vector<TcpTarget>& tcp) {
    std::vector<PmtuTarget> out;
    for (const auto& t : tcp) out.push_back({t.name, t.host, 33434});
    return out;
}

static std::vector<IcmpTarget> default_icmp_targets(const std::vector<TcpTarget>& tcp,
                                                    int interval_ms) {
    std::vector<IcmpTarget> out;
    for (const auto& t : tcp) out.push_back({t.name, t.host, interval_ms, 2000});
    return out;
}

static std::string first_resolver() {
    std::ifstream in("/etc/resolv.conf");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("nameserver", 0) == 0) {
            std::istringstream iss(line);
            std::string tag, ip;
            iss >> tag >> ip;
            if (!ip.empty()) return ip;
        }
    }
    return "1.1.1.1";
}

static int cmd_run_parsed(int duration_s, const std::string& out_dir, const std::string& profile,
                          int interval_ms, bool enable_dns, bool enable_icmp, bool enable_pmtu,
                          bool enable_netlink) {
    std::filesystem::create_directories(out_dir);
    std::string run_id = uuid4();
    auto targets = default_targets(profile);
    auto dns_targets = enable_dns ? default_dns_targets(interval_ms) : std::vector<DnsTarget>{};
    auto pmtu_targets = enable_pmtu ? default_pmtu_targets(targets) : std::vector<PmtuTarget>{};
    auto icmp_targets =
        enable_icmp ? default_icmp_targets(targets, interval_ms) : std::vector<IcmpTarget>{};
    write_manifest(out_dir, run_id, duration_s, profile, targets, dns_targets, pmtu_targets,
                   icmp_targets, interval_ms);

    EventBus bus;
    JsonlStore store(out_dir + "/events.jsonl");
    bus.add_sink(&store);

    Reactor reactor;
    TimerScheduler scheduler;
    TimerScheduler pmtu_scheduler;
    TcpConnectProbe tcp_probe(bus, run_id);
    DnsProbe dns_probe(bus, run_id);
    IcmpProbe icmp_probe(bus, run_id);
    NetlinkMonitor nl(bus, run_id);
    PmtuProbe pmtu_probe(bus, run_id);
    dns_probe.set_resolver(first_resolver());
    if (enable_netlink) nl.start(reactor);
    scheduler.start(reactor, interval_ms, [&]() {
        for (const auto& t : targets) tcp_probe.start(reactor, interval_ms, {t});
        if (enable_dns) dns_probe.tick(reactor, dns_targets);
        if (enable_icmp && icmp_probe.can_run()) icmp_probe.tick(reactor, icmp_targets);
    });
    if (enable_pmtu) pmtu_scheduler.start(reactor, 30000, [&]() { pmtu_probe.tick(pmtu_targets); });

    auto start = std::chrono::steady_clock::now();
    while (true) {
        reactor.loop_once(200);
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        if (elapsed >= duration_s) break;
    }
    scheduler.stop();
    if (enable_pmtu) pmtu_scheduler.stop();
    tcp_probe.stop();
    if (enable_dns) dns_probe.sweep_timeouts();
    if (enable_netlink) nl.stop();
    return 0;
}

static int cmd_report(const std::string& in_dir, const std::string& out_path) {
    ReportStats stats;
    if (!generate_report(in_dir, out_path, stats)) {
        std::cerr << "failed to generate report\n";
        return 1;
    }
    std::cout << "Report written to " << out_path << "\n";
    return 0;
}

static void print_usage() {
    std::cerr << "Usage: irr <run|report|doctor> [options]\n"
              << "  run    --duration <sec> --out <dir> --profile <name> --interval <ms> "
                 "[--no-dns] [--no-icmp] [--no-pmtu] [--no-netlink]\n"
              << "  report --in <bundle> --out <report.html>\n"
              << "  doctor (no args)\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h") {
        print_usage();
        return 0;
    }
    if (cmd == "doctor") return cmd_doctor();
    if (cmd == "run") {
        int duration_s = 600;
        std::string out_dir = "./bundle";
        std::string profile = "home";
        int interval_ms = 1000;
        bool enable_dns = true, enable_icmp = true, enable_pmtu = true, enable_netlink = true;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--duration" && i + 1 < argc) {
                duration_s = std::stoi(argv[++i]);
            } else if (a == "--out" && i + 1 < argc) {
                out_dir = argv[++i];
            } else if (a == "--profile" && i + 1 < argc) {
                profile = argv[++i];
            } else if ((a == "--interval" || a == "--interval-ms") && i + 1 < argc) {
                interval_ms = std::stoi(argv[++i]);
            } else if (a == "--no-dns") {
                enable_dns = false;
            } else if (a == "--no-icmp") {
                enable_icmp = false;
            } else if (a == "--no-pmtu") {
                enable_pmtu = false;
            } else if (a == "--no-netlink") {
                enable_netlink = false;
            }
        }
        return cmd_run_parsed(duration_s, out_dir, profile, interval_ms, enable_dns, enable_icmp,
                              enable_pmtu, enable_netlink);
    }
    if (cmd == "report") {
        std::string in_dir = "./bundle";
        std::string out = "./bundle/report.html";
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--in" && i + 1 < argc)
                in_dir = argv[++i];
            else if (a == "--out" && i + 1 < argc)
                out = argv[++i];
        }
        return cmd_report(in_dir, out);
    }
    std::cerr << "Unknown command\n";
    return 1;
}
