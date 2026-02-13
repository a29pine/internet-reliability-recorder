# Internet Reliability Recorder (irr)
Linux-first daemon + CLI that captures evidence-quality reliability incidents. irr continuously measures TCP connect, DNS (with TCP fallback), ICMP echo (when CAP_NET_RAW is available), PMTU ceilings, and netlink route/link changes. Results are stored as JSONL and rendered into a self-contained HTML report.

## Features
- Event-driven probes: TCP connect latency/loss, DNS health, ICMP RTT, PMTU ceilings, netlink link/route change markers
- JSONL evidence log plus manifest for reproducibility
- Offline HTML report with p50/p95/p99, loss, per-target breakdown, and timeline
- CLI doctor mode for quick environment checks (resolv.conf, CAP_NET_RAW)
- Toggle probes and intervals to adapt to constrained hosts (routers, SBCs)

## Requirements
- Linux with epoll and timerfd
- C++17 toolchain (g++/clang++)
- CMake >= 3.15
- Optional: CAP_NET_RAW for ICMP probe

## Quickstart
```bash
# Configure and build (Release by default)
./scripts/build.sh

# Run a capture for 60s
./build/irr run \
    --duration 60 \
    --out ./bundle \
    --profile home \
    --interval 1000 \
    [--no-dns] [--no-icmp] [--no-pmtu] [--no-netlink]

# Generate a report from the bundle
./build/irr report --in ./bundle --out ./bundle/report.html

# Environment doctor
./build/irr doctor
```

See docs/raspi.md and packaging/openwrt/README.md for router/SBC notes.

## Build, Test, Format
- Build: `./scripts/build.sh` (env `BUILD_TYPE=Debug|Release`, `BUILD_DIR=...`)
- Test: `./scripts/test.sh`
- Format: `./scripts/format.sh` (requires clang-format)
- Lint: `./scripts/lint.sh` (requires clang-tidy, uses compile_commands from build)

Out-of-source builds are required; artifacts live in `build/` by default.

## CLI
- `run`: start probes for a duration, write bundle (manifest + events.jsonl)
- `report`: read a bundle and emit self-contained HTML
- `doctor`: check resolver and CAP_NET_RAW availability

Key flags for `run`:
- `--duration <sec>` (default 600)
- `--out <dir>` (default ./bundle)
- `--profile <home|default>`
- `--interval <ms>` (probe interval; TCP/ICMP inherit)
- `--no-dns`, `--no-icmp`, `--no-pmtu`, `--no-netlink` to disable specific probes

## Data Model
- Manifest: `run.json` (run id, start time, profile, intervals, target lists)
- Events: `events.jsonl` (one JSON per event)
    - `probe.tcp.connect`, `probe.dns.result|timeout`, `probe.icmp.rtt|timeout`, PMTU, netlink

## Reporting
`irr report` parses `events.jsonl`, computes aggregates, and emits a portable HTML file (inline CSS/SVG). The report escapes all user-controlled strings to avoid injection when inspecting bundles.

## Architecture Overview
- Reactor (epoll + timerfd) drives probes
- EventBus fan-outs events to sinks (JSONL store)
- Probes emit structured events with timestamps, result metrics, and error categories
- Report generator converts JSONL into stats and SVG timeline

## Roadmap (short)
- Enrich DNS classification (EDNS, TCP retries)
- Optional SRTT smoothing for timeline
- Prometheus/text-based exporter

## Contributing
See CONTRIBUTING.md for workflow, coding style, and testing guidance. Please run format/lint/test before sending PRs.

## Security
See SECURITY.md for reporting instructions. The report generator escapes user-controlled strings; do not serve raw bundles without sanitization.

## License
MIT (see LICENSE).
