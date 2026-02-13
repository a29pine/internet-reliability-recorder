# Metrics
- `probe.tcp.connect`: connect RTT ms, ok flag, SO_ERROR category on failure.
- `probe.dns.result` / `probe.dns.timeout`: UDP query RTT, RCODE on error, TCP fallback result.
- `probe.pmtu.result`: discovered MTU (bytes) or `emsgsize` when constrained.
- `sys.netlink.route_change` / `sys.netlink.link_change`: link/route churn markers.
- `probe.icmp.rtt` / `probe.icmp.timeout`: echo RTT ms or timeout (requires CAP_NET_RAW).
- Percentiles: p50/p95/p99 via linear interpolation.
- Loss% = failures / total.
- Timebase: CLOCK_MONOTONIC (ns) plus wall-clock ISO8601.

Limitations:
- ICMP still stubbed unless CAP_NET_RAW added.
- DNS uses the first resolver from resolv.conf and a minimal parser.
- PMTU probing is lightweight and may be rate-limited by networks.
