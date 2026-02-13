#!/usr/bin/env bash
set -euo pipefail
# Best-effort integration: create netns with 50ms delay and 5% loss
NS=irrns
BUNDLE=/tmp/irr_bundle
sudo ip netns del $NS 2>/dev/null || true
sudo ip netns add $NS
sudo ip link add veth0 type veth peer name veth1
sudo ip link set veth1 netns $NS
sudo ip addr add 10.0.0.1/24 dev veth0
sudo ip link set veth0 up
sudo ip netns exec $NS ip addr add 10.0.0.2/24 dev veth1
sudo ip netns exec $NS ip link set veth1 up
sudo tc qdisc add dev veth0 root netem delay 50ms loss 5%
sudo ./irr run --duration 5 --out $BUNDLE --profile home
sudo ip netns del $NS
echo "Bundle at $BUNDLE"
