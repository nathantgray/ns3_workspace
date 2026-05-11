#!/usr/bin/env bash
set -euo pipefail

# Configure tap devices and bridges for example.py, then run ns-3.

if [[ ${EUID} -ne 0 ]]; then
  echo "Run as root: sudo $0"
  exit 1
fi

# Must match TapBridge device names in example.py
TAP1="tap1"
TAP2="tap2"
TAP3="tap3"

# Bridge names for each external link
BR1="br-tap1"
BR2="br-tap2"
BR3="br-tap3"

# Physical interfaces to connect.
# Override via environment, e.g.:
#   sudo PHY1=ens4 PHY2=ens5 PHY3=ens6 ./scripts/run_example.sh
PHY1="${PHY1:-ens4}"
PHY2="${PHY2:-ens5}"
PHY3="${PHY3:-ens6}"

iface_exists() {
  local iface="$1"
  [[ -n "$iface" ]] && ip link show "$iface" >/dev/null 2>&1
}

create_tap() {
  local tap="$1"
  # Recreate taps to avoid stale state after previous crashes.
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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="$ROOT_DIR/.venv/bin/python"
NS3_LIB_DIR="$ROOT_DIR/.venv/lib/python3.12/site-packages/ns3/lib64"
NS3_LIBEXEC_DIR="$ROOT_DIR/.venv/lib/python3.12/site-packages/ns3/libexec/ns3"
EXAMPLE_SCRIPT="$ROOT_DIR/example.py"

if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "Python interpreter not found or not executable: $PYTHON_BIN"
  exit 1
fi

if [[ ! -f "$EXAMPLE_SCRIPT" ]]; then
  echo "Simulation script not found: $EXAMPLE_SCRIPT"
  exit 1
fi

if [[ ! -d "$NS3_LIBEXEC_DIR" ]]; then
  echo "ns-3 libexec directory not found: $NS3_LIBEXEC_DIR"
  exit 1
fi

echo "Starting example.py..."
env LD_LIBRARY_PATH="$NS3_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  PATH="$NS3_LIBEXEC_DIR${PATH:+:$PATH}" \
  "$PYTHON_BIN" "$EXAMPLE_SCRIPT"

