#!/usr/bin/env bash
set -euo pipefail

# Modern replacement for tunctl/ifconfig/brctl setup in example.cc
# Creates TAP devices and Linux bridges, then attaches TAP + physical NIC.

if [[ ${EUID} -ne 0 ]]; then
  echo "Run as root: sudo $0"
  exit 1
fi

# Must match TapBridge device names in example.py/example.cc
TAP_LEFT="mytap1"
TAP_RIGHT="mytap2"

# Bridge names
BR_LEFT="mybridge"
BR_RIGHT="yourbridge"

# Physical interfaces to connect.
# Override via environment, e.g.:
#   sudo LEFT_PHY=eth0 RIGHT_PHY=eth1 ./scripts/setup_tap_bridges.sh
LEFT_PHY="${LEFT_PHY:-eth0}"
RIGHT_PHY="${RIGHT_PHY:-}"

iface_exists() {
  local iface="$1"
  [[ -n "$iface" ]] && ip link show "$iface" >/dev/null 2>&1
}

create_tap() {
  local tap="$1"
  if ! ip link show "$tap" >/dev/null 2>&1; then
    ip tuntap add dev "$tap" mode tap
  fi
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

create_tap "$TAP_LEFT"
create_tap "$TAP_RIGHT"

create_bridge "$BR_LEFT"
create_bridge "$BR_RIGHT"

enslave_if_not_member "$TAP_LEFT" "$BR_LEFT"
if iface_exists "$LEFT_PHY"; then
  enslave_if_not_member "$LEFT_PHY" "$BR_LEFT"
else
  echo "Warning: LEFT_PHY '$LEFT_PHY' does not exist; left bridge will only contain $TAP_LEFT"
fi

enslave_if_not_member "$TAP_RIGHT" "$BR_RIGHT"
if iface_exists "$RIGHT_PHY"; then
  enslave_if_not_member "$RIGHT_PHY" "$BR_RIGHT"
elif [[ -n "$RIGHT_PHY" ]]; then
  echo "Warning: RIGHT_PHY '$RIGHT_PHY' does not exist; right bridge will only contain $TAP_RIGHT"
else
  echo "Info: RIGHT_PHY not set; right bridge will only contain $TAP_RIGHT"
fi

echo "Done."
echo "$TAP_LEFT <-> $BR_LEFT <-> $LEFT_PHY"
if [[ -n "$RIGHT_PHY" ]]; then
  echo "$TAP_RIGHT <-> $BR_RIGHT <-> $RIGHT_PHY"
else
  echo "$TAP_RIGHT <-> $BR_RIGHT"
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="$ROOT_DIR/.venv/bin/python"
NS3_LIB_DIR="$ROOT_DIR/.venv/lib/python3.12/site-packages/ns3/lib64"
EXAMPLE_SCRIPT="$ROOT_DIR/example.py"

if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "Python interpreter not found or not executable: $PYTHON_BIN"
  exit 1
fi

if [[ ! -f "$EXAMPLE_SCRIPT" ]]; then
  echo "Simulation script not found: $EXAMPLE_SCRIPT"
  exit 1
fi

if [[ ! -d "$NS3_LIB_DIR" ]]; then
  echo "ns-3 library directory not found: $NS3_LIB_DIR"
  exit 1
fi

echo "Starting example.py..."
env LD_LIBRARY_PATH="$NS3_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  "$PYTHON_BIN" "$EXAMPLE_SCRIPT"

