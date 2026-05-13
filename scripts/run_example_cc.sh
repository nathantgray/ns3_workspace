#!/usr/bin/env bash
# run_example_cc.sh
# Sets up bridges and runs the compiled C++ ns-3 example.
# Usage: sudo ./scripts/run_example_cc.sh

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
    echo "Run as root: sudo $0"
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NS3_INSTALL="$ROOT_DIR/ns3-install"
NS3_LIB_DIR="$NS3_INSTALL/lib"
NS3_LIBEXEC_DIR="$NS3_INSTALL/libexec/ns3"
EXAMPLE_BIN="$ROOT_DIR/ns-allinone-3.44/ns-3.44/build/scratch/ns3.44-example-optimized"

BR1="br-tap1"; BR2="br-tap2"; BR3="br-tap3"
PHY1="${PHY1:-ens4}"; PHY2="${PHY2:-ens5}"; PHY3="${PHY3:-ens6}"

iface_exists() { [[ -n "$1" ]] && ip link show "$1" >/dev/null 2>&1; }

create_bridge() {
    local br="$1"
    if ! ip link show "$br" >/dev/null 2>&1; then
        ip link add name "$br" type bridge
    fi
    # Disable bridge MAC filtering so ns-3 ARP replies pass through
    echo 0 > /sys/class/net/$br/bridge/multicast_snooping
    ip link set dev "$br" up
}

enslave_if_not_member() {
    local iface="$1" br="$2" master
    master="$(ip -o link show "$iface" | sed -n 's/.* master \([^ ]*\).*/\1/p')"
    [[ "$master" != "$br" ]] && ip link set dev "$iface" master "$br"
    ip link set dev "$iface" up
}

# Clean up any leftover tap devices from previous runs
# ns-3 creates these itself in UseLocal mode
for tap in tap1 tap2 tap3; do
    if ip link show "$tap" >/dev/null 2>&1; then
        ip link set dev "$tap" down 2>/dev/null || true
        ip link del "$tap" 2>/dev/null || true
        echo "Removed leftover $tap"
    fi
done

# Create bridges and enslave physical interfaces
create_bridge "$BR1"
create_bridge "$BR2"
create_bridge "$BR3"

iface_exists "$PHY1" && enslave_if_not_member "$PHY1" "$BR1" \
    || echo "Warning: PHY1 '$PHY1' missing"
iface_exists "$PHY2" && enslave_if_not_member "$PHY2" "$BR2" \
    || echo "Warning: PHY2 '$PHY2' missing"
iface_exists "$PHY3" && enslave_if_not_member "$PHY3" "$BR3" \
    || echo "Warning: PHY3 '$PHY3' missing"

# Disable reverse path filtering so ARP replies are not dropped
sysctl -w net.ipv4.conf.all.rp_filter=0 >/dev/null
sysctl -w net.bridge.bridge-nf-call-iptables=0 >/dev/null 2>&1 || true
sysctl -w net.bridge.bridge-nf-call-arptables=0 >/dev/null 2>&1 || true

echo "Done."
echo "br-tap1 <-> $PHY1"
echo "br-tap2 <-> $PHY2"
echo "br-tap3 <-> $PHY3"
echo "(tap1/tap2/tap3 will be created by ns-3)"

if [[ ! -x "$EXAMPLE_BIN" ]]; then
    echo "Compiled binary not found: $EXAMPLE_BIN"
    echo "Build with: ./scripts/build_example.sh"
    exit 1
fi

if [[ ! -e "$NS3_LIBEXEC_DIR/tap-creator" ]]; then
    ln -sf ns3.44-tap-creator-optimized "$NS3_LIBEXEC_DIR/tap-creator"
fi

echo "Starting example.cc..."
export LD_LIBRARY_PATH="$NS3_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$NS3_LIBEXEC_DIR:$PATH"
stdbuf -oL -eL "$EXAMPLE_BIN" 2>&1