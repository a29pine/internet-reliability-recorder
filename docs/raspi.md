# Raspberry Pi notes
- Build natively: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`.
- Prefer lightweight desktops (i3/XFCE) or headless; keep bundle output on tmpfs to reduce SD wear.
- Disable optional features: `-DIRR_ENABLE_PCAP=OFF` (default). ICMP requires `sudo setcap cap_net_raw+ep ./irr`.
- For continuous runs, use systemd service with hardened options (see packaging/systemd/irr.service) and write bundles to /var/lib/irr on ext4, not FAT.
