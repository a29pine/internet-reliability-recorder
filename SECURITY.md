# Security

- Least privilege: TCP connect/DNS/PMTU probes use unprivileged sockets; ICMP remains optional with CAP_NET_RAW.
- Netlink monitor is read-only to observe route/link changes.
- Hardened run suggested: run as non-root, no writable dirs beyond bundle output.
- systemd unit example: packaging/systemd/irr.service (with NoNewPrivileges, PrivateDevices, ProtectSystem=strict).
