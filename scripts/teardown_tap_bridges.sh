#!/usr/bin/env bash
set -euo pipefail

# Undo TAP/bridge setup for the ns-3 TapBridge example.

if [[ ${EUID} -ne 0 ]]; then
  echo "Run as root: sudo $0"
  exit 1
fi

TAP_LEFT="mytap1"
TAP_RIGHT="mytap2"
BR_LEFT="mybridge"
BR_RIGHT="yourbridge"
LEFT_PHY="enp3s0"
RIGHT_PHY="enx7cc2c6331c3c"

ip link set dev "$LEFT_PHY" nomaster 2>/dev/null || true
ip link set dev "$RIGHT_PHY" nomaster 2>/dev/null || true
ip link set dev "$TAP_LEFT" nomaster 2>/dev/null || true
ip link set dev "$TAP_RIGHT" nomaster 2>/dev/null || true

ip link del "$BR_LEFT" type bridge 2>/dev/null || true
ip link del "$BR_RIGHT" type bridge 2>/dev/null || true

ip link del "$TAP_LEFT" 2>/dev/null || true
ip link del "$TAP_RIGHT" 2>/dev/null || true

echo "Teardown complete."
