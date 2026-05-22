# Bridging Multiple VMs Through an ns-3 Simulation

This document explains how to connect multiple real virtual machines through an ns-3 simulated network using the `TapBridge` NetDevice in `UseBridge` mode. The simulation acts as a shared L2 segment between the VMs, allowing them to communicate as if they were on the same Ethernet network.

This guide uses a concrete example with **3 VMs plus a router VM running ns-3**, but the approach generalizes to any number of endpoints.

---

## 1. Overview

### What We're Building

- **3 "endpoint" VMs** (`VM1`, `VM2`, `VM3`) that need to communicate with each other.
- **1 "router" VM** (`ns3-router`) that runs an ns-3 simulation acting as the network between them.
- **3 dedicated L2 links**, one from each endpoint VM to `ns3-router` (separate virtual NICs / isolated virtual networks in your hypervisor).

Inside the ns-3 simulation, the three links are joined into a single shared CSMA channel, so the three endpoints end up on one flat L2 broadcast domain (`10.0.0.0/24`).

### Why UseBridge Mode

The ns-3 `TapBridge` has three modes: `ConfigureLocal`, `UseLocal`, and `UseBridge` [1].

- `ConfigureLocal` makes ns-3 create the tap and embeds the simulated node's MAC/IP on the Linux host itself — useful when the host *is* the endpoint [1].
- `UseLocal` supports only a single MAC on the Linux side and is "incompatible" with Linux bridges containing more than one net device [1].
- `UseBridge` "logically extend[s] a Linux bridge into ns-3" [1]. You create an OS bridge containing a tap device and your real NIC, and the TapBridge joins that bridge to an ns-3 net device.

Because each of our real NICs on `ns3-router` must be joined with a tap and bridged into the simulation, and because traffic carries the real MACs of the remote VMs (i.e., multiple source MACs on the Linux side), **UseBridge is the correct mode** [1].

### Why CSMA

`UseBridge` mode requires that the bridged ns-3 net device "support `SendFrom()` and have a hookable promiscuous receive callback" [1]. CSMA satisfies both, so we use a CSMA channel inside ns-3.

---

## 2. Architecture

```
VM1 (10.0.0.1/24)      VM2 (10.0.0.2/24)      VM3 (10.0.0.3/24, Windows)
      |                      |                       |
   [L2 link]              [L2 link]               [L2 link]
      |                      |                       |
    ens4                   ens5                    ens6     [on ns3-router]
      |                      |                       |
     br0                    br1                     br2     (Linux bridges, no IP)
        \                  /                       /   
        tap0            tap1                   tap2
          \                |                    /
           \               |                   /
     +-------------------------------------------------+
     |   ns-3 simulation (RealtimeSimulatorImpl)       |
     |                                                 |
     |   ghost0         ghost1         ghost2          |
     |  [TapBridge]    [TapBridge]    [TapBridge]      |
     |  [CSMA dev]     [CSMA dev]     [CSMA dev]       |
     |       \             |             /             |
     |       +--- single CSMA channel ---+             |
     +-------------------------------------------------+
```

Each TapBridge in UseBridge mode extends one OS bridge (`br0`/`br1`/`br2`) into the ns-3 simulation [1]. The shared CSMA channel then joins all three segments into a single broadcast domain, so any VM can reach any other directly at L2.

The bridges on `ns3-router` have **no IP addresses**, and neither do the real interfaces (`ens4`/`ens5`/`ens6`) or the taps — `ns3-router` itself is not a participant in the `10.0.0.0/24` network; it only relays frames.

---

## 3. Prerequisites

On **ns3-router**:

- Ubuntu (or similar Linux) with `ip`, `iproute2`, and kernel modules `tun` and `bridge`.
- ns-3 built with the `tap-bridge` module (tested with ns-3.44).
- Root / `sudo` access.
- At least one additional management interface (e.g., `ens3`) that is **not** part of the ns-3 bridging, so you can still SSH in.

On the **endpoint VMs**:

- Any OS is fine (Linux, Windows, etc.) — `TapBridge` does not care what sits on the other side of the OS bridge [1].
- A NIC connected to the dedicated L2 link toward `ns3-router`.

---

## 4. Identify the Interfaces

On `ns3-router`, list interfaces and decide which ones face the endpoint VMs. Example:

```bash
ip -brief link show
```

Sample output:
```
lo     UNKNOWN  00:00:00:00:00:00
ens3   UP       02:01:01:61:00:16   # management — leave alone
ens4   UP       02:01:01:76:00:01   # -> VM1
ens5   UP       02:01:01:78:00:01   # -> VM2
ens6   UP       02:01:01:77:00:01   # -> VM3
```

Make sure these interfaces have **no IP address** assigned (we will flush them in the setup script anyway).

---

## 5. Host-Side Setup on ns3-router

Save the following as `setup-bridges.sh` on `ns3-router` and run it with `sudo bash setup-bridges.sh`. Adjust the `MAP` variable if your interface names differ.

```bash
#!/usr/bin/env bash
set -e

# Map real interface -> "tap bridge"
declare -A MAP=( [ens4]="tap0 br0" [ens5]="tap1 br1" [ens6]="tap2 br2" )

for IF in "${!MAP[@]}"; do
    read TAP BR <<< "${MAP[$IF]}"
    echo "=== Setting up $IF <-> $TAP in $BR ==="

    # Bridge with no IP (ns3-router is not a participant in 10.0.0.0/24)
    ip link add name "$BR" type bridge 2>/dev/null || true
    ip link set dev "$BR" up

    # Tap device
    ip tuntap add mode tap "$TAP" 2>/dev/null || true
    ip link set dev "$TAP" up promisc on

    # Real NIC: no IP, promiscuous, up
    ip addr flush dev "$IF" || true
    ip link set dev "$IF" up promisc on

    # Enslave both into the bridge
    ip link set dev "$TAP" master "$BR"
    ip link set dev "$IF"  master "$BR"
done

echo "Done."
ip -brief link show type bridge
bridge link
```

To tear everything down later:

```bash
for B in br0 br1 br2; do sudo ip link del "$B" 2>/dev/null; done
for T in tap0 tap1 tap2; do sudo ip link del "$T" 2>/dev/null; done
```

This matches the structure shown in the ns-3 documentation for `UseBridge` mode, where a bridge contains a tap and one or more real devices and the TapBridge then logically extends that bridge into ns-3 [1].

---

## 6. The ns-3 Program

Save as `scratch/tap-3vm-l2.cc` in your ns-3 source tree, then `./ns3 build`.

```cpp
// scratch/tap-3vm-l2.cc
//
// L2-bridge three real VMs into a single ns-3 CSMA segment via TapBridge
// UseBridge mode. Each real NIC on ns3-router (ens4/ens5/ens6) has been
// pre-joined with a tap (tap0/tap1/tap2) inside an OS bridge (br0/br1/br2)
// by setup-bridges.sh.
//
// Run as root:  sudo ./ns3 run tap-3vm-l2

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Tap3VmL2");

int main(int argc, char* argv[])
{
    // Emulation requires real-time scheduling and checksum computation.
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // One "ghost" node per bridged VM. These nodes exist only to hold the
    // TapBridge and the bridged CSMA device. They do not need an IP stack.
    NodeContainer ghosts;
    ghosts.Create(3);

    // Single shared CSMA channel — one L2 broadcast domain for all 3 VMs.
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));

    NetDeviceContainer devs = csma.Install(ghosts);

    // Attach a TapBridge in UseBridge mode to each ghost's CSMA device.
    // DeviceName refers to the pre-created tap on the host; that tap has
    // already been placed into an OS bridge together with a real NIC.
    TapBridgeHelper tap;
    tap.SetAttribute("Mode", StringValue("UseBridge"));

    tap.SetAttribute("DeviceName", StringValue("tap0"));
    tap.Install(ghosts.Get(0), devs.Get(0));

    tap.SetAttribute("DeviceName", StringValue("tap1"));
    tap.Install(ghosts.Get(1), devs.Get(1));

    tap.SetAttribute("DeviceName", StringValue("tap2"));
    tap.Install(ghosts.Get(2), devs.Get(2));

    // Optional pcap on the ns-3 CSMA side for debugging:
    // csma.EnablePcapAll("tap-3vm-l2", true);

    Simulator::Stop(Seconds(600.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
```

A few notes on the code:

- The ghost nodes have no IP stack installed. The ns-3 documentation notes that the bridged device must not have an IP address under UseBridge-style integration, and that the node that holds the TapBridge is essentially a "ghost" whose only purpose is to host the bridged device [1].
- `ChecksumEnabled` must be true for emulation because the real kernel expects correct L4 checksums on outbound packets.
- `RealtimeSimulatorImpl` makes simulation time track wall-clock time, required whenever ns-3 exchanges packets with real systems.

---

## 7. Configure the Endpoint VMs

All three endpoints are on the same `/24`, so **no default gateway and no static routes are needed** — traffic between them is pure L2 via ARP, and `ns3-router` itself does not appear at L3.

### 7.1 Linux endpoint (VM1 example)

```bash
# Replace ethX with the interface connected to ns3-router
sudo ip addr flush dev ethX
sudo ip addr add 10.0.0.1/24 dev ethX
sudo ip link set ethX up
```

Repeat for VM2 with `10.0.0.2/24`.
