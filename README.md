# ns-3 Hardware-in-the-Loop on WSL2

## Prerequisites

- Ubuntu on WSL2
- sudo access

## 1. Install system dependencies

```bash
sudo apt update
sudo apt install clang graphviz libgraphviz-dev pkg-config libffi-dev
```

## 2. Create project

```bash
mkdir ns3_workspace && cd ns3_workspace
uv init --no-workspace
```

Edit `pyproject.toml` — change `requires-python` to `>=3.12`.

```bash
uv python pin 3.12
uv add ns3
```

## 3. Fix hardcoded helper binary path

The pip package hardcodes a build-machine path for the tap-device-creator binary. Symlink it:

```bash
sudo mkdir -p /gcl-builds/build/src/tap-bridge/
sudo ln -s $(pwd)/.venv/lib/python3.12/site-packages/ns3/libexec/ns3/ns3.44-tap-creator \
  /gcl-builds/build/src/tap-bridge/ns3.44-tap-creator
```

## 4. Ensure /dev/net/tun exists

```bash
ls /dev/net/tun
```

If missing:

```bash
sudo mkdir -p /dev/net
sudo mknod /dev/net/tun c 10 200
sudo chmod 666 /dev/net/tun
```

## 5. Create the simulation

`hil.py`:

```python
from ns import ns

ns.GlobalValue.Bind("SimulatorImplementationType",
                     ns.StringValue("ns3::RealtimeSimulatorImpl"))
ns.GlobalValue.Bind("ChecksumEnabled", ns.BooleanValue(True))

nodes = ns.NodeContainer()
nodes.Create(4)

csma = ns.CsmaHelper()
csma.SetChannelAttribute("DataRate", ns.DataRateValue(ns.DataRate(5000000)))
csma.SetChannelAttribute("Delay", ns.TimeValue(ns.MilliSeconds(2)))
devices = csma.Install(nodes)

stack = ns.InternetStackHelper()
stack.Install(nodes)

address = ns.Ipv4AddressHelper()
address.SetBase(ns.Ipv4Address("10.1.1.0"), ns.Ipv4Mask("255.255.255.0"))
interfaces = address.Assign(devices)

tap = ns.TapBridgeHelper()
tap.SetAttribute("Mode", ns.StringValue("ConfigureLocal"))
tap.SetAttribute("DeviceName", ns.StringValue("tap-ns3"))
tap.Install(nodes.Get(0), devices.Get(0))

ns.Ipv4GlobalRoutingHelper.PopulateRoutingTables()

ns.Simulator.Stop(ns.Seconds(300.0))
ns.Simulator.Run()
ns.Simulator.Destroy()
```

## 6. Run

```bash
sudo env LD_LIBRARY_PATH="$(pwd)/.venv/lib/python3.12/site-packages/ns3/lib64" \
  $(pwd)/.venv/bin/python hil.py
```

## 7. Test (from another terminal)

```bash
ping 10.1.1.2   # node 1
ping 10.1.1.3   # node 2
ping 10.1.1.4   # node 3
```

The tap device `tap-ns3` (10.1.1.1) exists only while the simulation is running. All traffic to 10.1.1.0/24 flows through the ns-3 simulated CSMA network.
