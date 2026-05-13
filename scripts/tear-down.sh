for B in br0 br1 br2; do sudo ip link del "$B" 2>/dev/null; done
for T in tap0 tap1 tap2; do sudo ip link del "$T" 2>/dev/null; done