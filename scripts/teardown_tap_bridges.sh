#!/usr/bin/env bash
set -euo pipefail

# Undo TAP/bridge setup for the ns-3 TapBridge example.

if [[ ${EUID} -ne 0 ]]; then
  echo "Run as root: sudo $0"
  exit 1
fi

TAP1="tap1"
TAP2="tap2"
TAP3="tap3"
BR1="br-tap1"
BR2="br-tap2"
BR3="br-tap3"
PHY1="${PHY1:-ens4}"
PHY2="${PHY2:-ens5}"
PHY3="${PHY3:-ens6}"

ip link set dev "$PHY1" nomaster 2>/dev/null || true
ip link set dev "$PHY2" nomaster 2>/dev/null || true
ip link set dev "$PHY3" nomaster 2>/dev/null || true
ip link set dev "$TAP1" nomaster 2>/dev/null || true
ip link set dev "$TAP2" nomaster 2>/dev/null || true
ip link set dev "$TAP3" nomaster 2>/dev/null || true

ip link del "$BR1" type bridge 2>/dev/null || true
ip link del "$BR2" type bridge 2>/dev/null || true
ip link del "$BR3" type bridge 2>/dev/null || true

ip link del "$TAP1" 2>/dev/null || true
ip link del "$TAP2" 2>/dev/null || true
ip link del "$TAP3" 2>/dev/null || true

echo "Teardown complete."
