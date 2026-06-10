#!/usr/bin/env bash
set -e

# Tear down old
for B in br0 br1 br2 br3; do ip link del "$B" 2>/dev/null || true; done
for T in tap0 tap1 tap2 tap3; do ip link del "$T" 2>/dev/null || true; done

declare -A MAP=(
    [ens4]="tap0 br0"     # VM2 at 10.0.1.10
    [ens11]="tap1 br1"    # Shared network (ens11 has 192.168.253.160 and the OpalRT Device is at 192.168.252.127)
    [ens6]="tap2 br2"     # VM1 at 10.0.3.10
)

for IF in "${!MAP[@]}"; do
    read TAP BR <<< "${MAP[$IF]}"
    echo "=== Setting up $IF <-> $TAP in $BR ==="

    ip link add name "$BR" type bridge 2>/dev/null || true
    ip link set dev "$BR" up

    ip tuntap add mode tap "$TAP" 2>/dev/null || true
    ip link set dev "$TAP" up promisc on

    ip addr flush dev "$IF" || true
    ip link set dev "$IF" up promisc on

    ip link set dev "$TAP" master "$BR"
    ip link set dev "$IF"  master "$BR"
done

echo "Done."
bridge link