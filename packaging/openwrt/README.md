# OpenWrt packaging (plan)

Goals: ship a lean `irr` binary for routers/APs without burning flash or requiring root-only operation.

Build (OpenWrt SDK):
- `make menuconfig` → enable SDK, select your target; install SDK.
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIRR_ENABLE_PCAP=OFF -DCMAKE_TOOLCHAIN_FILE=$STAGING_DIR/toolchain.cmake`
- `cmake --build build`
- Install to staging: copy `build/irr` to `./staging_dir/target-*/root-*/usr/bin/irr` and package via standard OpenWrt `Makefile` (to be added).

Or use the skeleton package recipe at `packaging/openwrt/Makefile` inside the OpenWrt buildroot.

Runtime guidance:
- Default to `irr run --out /tmp/irr` to avoid flash wear; tmpfs is safer.
- ICMP requires `CAP_NET_RAW` (use `setcap` if supported; many OpenWrt builds ship `libcap`). If unavailable, TCP/DNS/PMTU still work.
- Netlink monitoring works if `NETLINK_ROUTE` allowed (typical). If blocked, events just won't emit.

Profile hints:
- Router mode: reduce intervals, e.g., `--duration 0 --profile home --out /tmp/irr --interval 2000` to save CPU.
- Keep optional features OFF: PCAP/eBPF remain disabled unless explicitly enabled at build time.

Warnings:
- Avoid continuous writes to flash (use /tmp or remote export of the bundle).
- BusyBox shells may lack `ip netns` and `tc`; integration script is Linux desktop/server oriented.

Minimal dependencies: libc, libpthread; no heavy libs by default.
