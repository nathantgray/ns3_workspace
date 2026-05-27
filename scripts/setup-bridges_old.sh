#!/usr/bin/env bash
set -e

# Interface <-> tap/bridge mapping
declare -A MAP=( [ens4]="tap0 br0" [ens5]="tap1 br1" [ens6]="tap2 br2" )

for IF in "${!MAP[@]}"; do
    read TAP BR <<< "${MAP[$IF]}"
    echo "=== Setting up $IF <-> $TAP in $BR ==="

    # Create bridge (no IP — ns3-router is not a participant in the L2 segment)
    ip link add name "$BR" type bridge 2>/dev/null || true
    ip link set dev "$BR" up

    # Create tap
    ip tuntap add mode tap "$TAP" 2>/dev/null || true
    ip link set dev "$TAP" up promisc on

    # Real interface must be up and promiscuous (no IP on it either)
    ip addr flush dev "$IF" || true
    ip link set dev "$IF" up promisc on

    # Enslave both into the bridge
    ip link set dev "$TAP" master "$BR"
    ip link set dev "$IF"  master "$BR"
done

echo "Done. Current state:"
ip -brief link show type bridge
bridge link