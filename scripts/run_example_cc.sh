#!/usr/bin/env bash
# run_example_cc.sh
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

PHY1="${PHY1:-ens4}"
PHY2="${PHY2:-ens5}"
PHY3="${PHY3:-ens6}"

# Clean up everything from previous runs
for tap in tap1 tap2 tap3; do
    if ip link show "$tap" >/dev/null 2>&1; then
        ip link set dev "$tap" down 2>/dev/null || true
        ip link del "$tap" 2>/dev/null || true
        echo "Removed leftover $tap"
    fi
done

for br in br-tap1 br-tap2 br-tap3; do
    if ip link show "$br" >/dev/null 2>&1; then
        ip link set dev "$br" down 2>/dev/null || true
        ip link del "$br" 2>/dev/null || true
        echo "Removed leftover $br"
    fi
done

if [[ ! -x "$EXAMPLE_BIN" ]]; then
    echo "Binary not found: $EXAMPLE_BIN"
    echo "Build with: ./scripts/build_example.sh"
    exit 1
fi

if [[ ! -e "$NS3_LIBEXEC_DIR/tap-creator" ]]; then
    ln -sf ns3.44-tap-creator-optimized "$NS3_LIBEXEC_DIR/tap-creator"
fi

echo "Starting example.cc..."
export LD_LIBRARY_PATH="$NS3_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$NS3_LIBEXEC_DIR:$PATH"
stdbuf -oL -eL "$EXAMPLE_BIN" 2>&1 &
NS3_PID=$!

# Wait for ns-3 to create the taps
echo "Waiting for ns-3 to create tap devices..."
for tap in tap1 tap2 tap3; do
    for i in $(seq 1 30); do
        if ip link show "$tap" >/dev/null 2>&1; then
            echo "  $tap created"
            break
        fi
        if ! kill -0 $NS3_PID 2>/dev/null; then
            echo "ERROR: ns-3 exited before creating $tap"
            exit 1
        fi
        sleep 0.5
    done
    if ! ip link show "$tap" >/dev/null 2>&1; then
        echo "ERROR: $tap was never created"
        exit 1
    fi
done

# Now bridge each tap with its physical interface
echo "Bridging taps with physical interfaces..."
for tap_phy in "tap1:$PHY1" "tap2:$PHY2" "tap3:$PHY3"; do
    tap="${tap_phy%%:*}"
    phy="${tap_phy##*:}"
    br="br-${tap}"

    ip link add name "$br" type bridge
    echo 0 > /sys/class/net/$br/bridge/multicast_snooping
    ip link set dev "$tap" master "$br"
    ip link set dev "$phy" master "$br"
    ip link set dev "$tap" up
    ip link set dev "$phy" up
    ip link set dev "$br" up
    echo "  $tap <-> $br <-> $phy"
done

echo "Network ready."
echo "Waiting for ns-3 to finish (PID $NS3_PID)..."
wait $NS3_PID