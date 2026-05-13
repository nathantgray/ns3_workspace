#!/usr/bin/env bash
# run_example_cc.sh
# Sets up tap devices and bridges, then runs the compiled C++ ns-3 example.
# Usage: sudo ./scripts/run_example_cc.sh

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
    echo "Run as root: sudo $0"
    exit 1
fi

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NS3_INSTALL="$ROOT_DIR/ns3-install"
NS3_LIB_DIR="$NS3_INSTALL/lib"
NS3_LIBEXEC_DIR="$NS3_INSTALL/libexec/ns3"
EXAMPLE_BIN="$ROOT_DIR/ns-allinone-3.44/ns-3.44/build/scratch/ns3.44-example-optimized"

# Must match TapBridge device names in example.cc [6]
TAP1="tap1"
TAP2="tap2"
TAP3="tap3"

# Bridge names
BR1="br-tap1"
BR2="br-tap2"
BR3="br-tap3"

# Physical interfaces — override via environment if needed
# e.g. sudo PHY1=eth0 PHY2=eth1 PHY3=eth2 ./scripts/run_example_cc.sh
PHY1="${PHY1:-ens4}"
PHY2="${PHY2:-ens5}"
PHY3="${PHY3:-ens6}"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
iface_exists() {
    local iface="$1"
    [[ -n "$iface" ]] && ip link show "$iface" >/dev/null 2>&1
}

create_tap() {
    local tap="$1"
    # Recreate taps to avoid stale state after previous crashes
    if ip link show "$tap" >/dev/null 2>&1; then
        ip link set dev "$tap" down 2>/dev/null || true
        ip link del "$tap" 2>/dev/null || true
    fi
    ip tuntap add dev "$tap" mode tap
    ip link set dev "$tap" promisc on
    ip link set dev "$tap" up
}

create_bridge() {
    local br="$1"
    if ! ip link show "$br" >/dev/null 2>&1; then
        ip link add name "$br" type bridge
    fi
    ip link set dev "$br" up
}

enslave_if_not_member() {
    local iface="$1"
    local br="$2"
    local master
    master="$(ip -o link show "$iface" | sed -n 's/.* master \([^ ]*\).*/\1/p')"
    if [[ "$master" != "$br" ]]; then
        ip link set dev "$iface" master "$br"
    fi
    ip link set dev "$iface" up
}

# ---------------------------------------------------------------------------
# Create taps and bridges
# ---------------------------------------------------------------------------
create_tap "$TAP1"
create_tap "$TAP2"
create_tap "$TAP3"

create_bridge "$BR1"
create_bridge "$BR2"
create_bridge "$BR3"

enslave_if_not_member "$TAP1" "$BR1"
if iface_exists "$PHY1"; then
    enslave_if_not_member "$PHY1" "$BR1"
else
    echo "Warning: PHY1 '$PHY1' does not exist; bridge $BR1 will only contain $TAP1"
fi

enslave_if_not_member "$TAP2" "$BR2"
if iface_exists "$PHY2"; then
    enslave_if_not_member "$PHY2" "$BR2"
else
    echo "Warning: PHY2 '$PHY2' does not exist; bridge $BR2 will only contain $TAP2"
fi

enslave_if_not_member "$TAP3" "$BR3"
if iface_exists "$PHY3"; then
    enslave_if_not_member "$PHY3" "$BR3"
else
    echo "Warning: PHY3 '$PHY3' does not exist; bridge $BR3 will only contain $TAP3"
fi

echo "Done."
echo "$TAP1 <-> $BR1 <-> $PHY1"
echo "$TAP2 <-> $BR2 <-> $PHY2"
echo "$TAP3 <-> $BR3 <-> $PHY3"

# ---------------------------------------------------------------------------
# Validate paths
# ---------------------------------------------------------------------------
if [[ ! -f "$EXAMPLE_BIN" ]]; then
    echo "Compiled binary not found: $EXAMPLE_BIN"
    echo "Build it first with:"
    echo "  cd ns-allinone-3.44/ns-3.44 && ./ns3 build scratch/example"
    exit 1
fi

if [[ ! -d "$NS3_LIBEXEC_DIR" ]]; then
    echo "ns-3 libexec directory not found: $NS3_LIBEXEC_DIR"
    exit 1
fi

# Ensure the unversioned tap-creator symlink exists
if [[ ! -e "$NS3_LIBEXEC_DIR/tap-creator" ]]; then
    ln -sf ns3.44-tap-creator-optimized "$NS3_LIBEXEC_DIR/tap-creator"
    echo "Created tap-creator symlink"
fi

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
echo "Starting example..."
export LD_LIBRARY_PATH="$NS3_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$NS3_LIBEXEC_DIR:$PATH"
"$EXAMPLE_BIN"