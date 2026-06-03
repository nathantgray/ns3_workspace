1) run `sudo bash scripts/setup-bridges.sh` to create the TAP bridges and enslave them to the physical NICs
2) build tap-3vm-routed by copying to the scratch directory and running `./ns3 build tap-3vm-routed`
3) run the simulation with `sudo ./ns3 run tap-3vm-routed`

# Network Setup
- 3 VMs are each connected to the central ns3-router VM via L2 links.
- 
ip -brief link show
sudo ip addr add 10.0.1.10/24 dev ens9
sudo ip link set ens9 up
sudo ip route add default via 10.0.1.2 dev ens9
# Setup on Linux VMs

# Setup on Windows VM
1) Add ip address to NIC connected to ns3-router 10.0.2.10/24 with default gateway 10.0.2.2. However this default gateway will likely be overridden by the higher priority default gateway for the other NIC, so you may need to add a static route for each destination IP.
    a) check routes with `route print -4`
    b) add static routes with `route add 10.0.1.0 mask 255.255.255.0 10.0.2.2 -p` and `route add 10.0.3.0 mask 255.255.255.0 10.0.2.2 -p`






# Verify ns3-router Network Setup

Run these commands on ns3-router after running `sudo bash setup-bridges.sh`:

## 1. Check bridges exist with correct members

```bash
bridge link
```

Expected:
```
ens4    master br0 state forwarding
tap0    master br0 state forwarding
ens6    master br2 state forwarding
tap2    master br2 state forwarding
ens11   master br3 state forwarding
tap3    master br3 state forwarding
```

## 2. Check all interfaces are UP and promiscuous

```bash
ip -brief link show | grep -E "br[0-3]|tap[0-3]|ens4|ens6|ens11"
```

Expected: all should show `UP` (taps will show `DOWN`/`NO-CARRIER` until ns-3 starts — that's normal).

## 3. Confirm NO IP addresses on bridges, taps, or bridged interfaces

```bash
for dev in br0 br2 br3 tap0 tap2 tap3 ens4 ens6 ens11; do
    echo "=== $dev ==="
    ip addr show dev $dev | grep inet || echo "  (no IP - correct)"
done
```

Every device should show `(no IP - correct)`.

## 4. Confirm management interface is untouched

```bash
ip addr show dev ens3 | grep inet
```

Should still show `192.168.117.221/24`.

## 5. Confirm ens5 is NOT in any bridge (no longer used)

```bash
bridge link show dev ens5 2>&1
```

Should show nothing or an error — `ens5` should not be enslaved anywhere.

## 6. Quick summary view

```bash
echo "=== Bridges ==="
ip -brief link show type bridge

echo "=== Taps ==="
ip -brief link show type tun

echo "=== Bridge membership ==="
bridge link

echo "=== All IPs ==="
ip -brief addr show
```

## After Starting ns-3

Once you run `sudo ./ns3 run tap-3vm-routed`, verify taps came up:

```bash
ip -brief link show type tun
```

All three (`tap0`, `tap2`, `tap3`) should now show `UP` with `LOWER_UP` in flags.