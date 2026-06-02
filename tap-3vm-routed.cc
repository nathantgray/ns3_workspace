// scratch/tap-3vm-routed.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tap-bridge-module.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Tap3VmRouted");
int main(int argc, char *argv[])
{
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // =========================================================
    // Network plan
    //
    //   VM0 (10.0.1.10) --- [tap0/bridge0] --- ghost0 --- internal0 --- central
    //   VM1 (10.0.2.10) --- [tap1/bridge1] --- ghost1 --- internal1 --- central
    //   VM2 (10.0.3.10) --- [tap2/bridge2] --- ghost2 --- internal2 --- central
    //
    //   CSMA segments (ghost <-> internal):
    //     10.0.1.0/24   internal0 = 10.0.1.1   ghost0 = no IP
    //     10.0.2.0/24   internal1 = 10.0.2.1   ghost1 = no IP
    //     10.0.3.0/24   internal2 = 10.0.3.1   ghost2 = no IP
    //
    //   P2P segments (internal <-> central):
    //     10.0.10.0/30  internal0 = 10.0.10.1  central = 10.0.10.2
    //     10.0.11.0/30  internal1 = 10.0.11.1  central = 10.0.11.2
    //     10.0.12.0/30  internal2 = 10.0.12.1  central = 10.0.12.2
    // =========================================================

    // =========================================================
    // Create nodes
    // =========================================================
    Ptr<Node> ghost0    = CreateObject<Node>();
    Ptr<Node> ghost1    = CreateObject<Node>();
    Ptr<Node> ghost2    = CreateObject<Node>();
    Ptr<Node> internal0 = CreateObject<Node>();
    Ptr<Node> internal1 = CreateObject<Node>();
    Ptr<Node> internal2 = CreateObject<Node>();
    Ptr<Node> central   = CreateObject<Node>();

    // =========================================================
    // Install IP stacks
    // =========================================================
    InternetStackHelper stack;
    stack.Install(ghost0);
    stack.Install(ghost1);
    stack.Install(ghost2);
    stack.Install(internal0);
    stack.Install(internal1);
    stack.Install(internal2);
    stack.Install(central);

    // =========================================================
    // Enable IP forwarding on internal and central nodes
    // =========================================================
    internal0->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    internal1->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    internal2->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    central->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));

    // =========================================================
    // CSMA links: ghost <-> internal
    // =========================================================
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay",    TimeValue(MilliSeconds(0)));

    // Each NetDeviceContainer: Get(0) = ghost side, Get(1) = internal side
    NetDeviceContainer csma0 = csma.Install(NodeContainer(ghost0, internal0));
    NetDeviceContainer csma1 = csma.Install(NodeContainer(ghost1, internal1));
    NetDeviceContainer csma2 = csma.Install(NodeContainer(ghost2, internal2));

    // =========================================================
    // P2P links: internal <-> central
    // =========================================================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate",  StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay",    StringValue("2ms"));

    // Each NetDeviceContainer: Get(0) = internal side, Get(1) = central side
    NetDeviceContainer p2p0 = p2p.Install(NodeContainer(internal0, central));
    NetDeviceContainer p2p1 = p2p.Install(NodeContainer(internal1, central));
    NetDeviceContainer p2p2 = p2p.Install(NodeContainer(internal2, central));

    // =========================================================
    // Assign IP addresses explicitly
    //
    // Ghost nodes are skipped — they are pure L2 bridge passthrough.
    // Assigning an IP to a ghost would cause it to respond to ARP
    // for addresses the real VM needs, breaking connectivity.
    // =========================================================
    Ipv4AddressHelper address;

    // --- CSMA segment 0: 10.0.1.0/24 ---
    // ghost0    = no IP
    // internal0 = 10.0.1.1
    {
        NetDeviceContainer dev;
        dev.Add(csma0.Get(1));  // internal0 side only
        address.SetBase("10.0.1.0", "255.255.255.0");
        address.Assign(dev);
    }

    // --- CSMA segment 1: 10.0.2.0/24 ---
    // ghost1    = no IP
    // internal1 = 10.0.2.1
    {
        NetDeviceContainer dev;
        dev.Add(csma1.Get(1));  // internal1 side only
        address.SetBase("10.0.2.0", "255.255.255.0");
        address.Assign(dev);
    }

    // --- CSMA segment 2: 10.0.3.0/24 ---
    // ghost2    = no IP
    // internal2 = 10.0.3.1
    {
        NetDeviceContainer dev;
        dev.Add(csma2.Get(1));  // internal2 side only
        address.SetBase("10.0.3.0", "255.255.255.0");
        address.Assign(dev);
    }

    // --- P2P segment 0: 10.0.10.0/30 ---
    // internal0 = 10.0.10.1
    // central   = 10.0.10.2
    address.SetBase("10.0.10.0", "255.255.255.252");
    Ipv4InterfaceContainer p2p0Ifaces = address.Assign(p2p0);
    // p2p0Ifaces.GetAddress(0) = 10.0.10.1 (internal0)
    // p2p0Ifaces.GetAddress(1) = 10.0.10.2 (central)

    // --- P2P segment 1: 10.0.11.0/30 ---
    // internal1 = 10.0.11.1
    // central   = 10.0.11.2
    address.SetBase("10.0.11.0", "255.255.255.252");
    Ipv4InterfaceContainer p2p1Ifaces = address.Assign(p2p1);
    // p2p1Ifaces.GetAddress(0) = 10.0.11.1 (internal1)
    // p2p1Ifaces.GetAddress(1) = 10.0.11.2 (central)

    // --- P2P segment 2: 10.0.12.0/30 ---
    // internal2 = 10.0.12.1
    // central   = 10.0.12.2
    address.SetBase("10.0.12.0", "255.255.255.252");
    Ipv4InterfaceContainer p2p2Ifaces = address.Assign(p2p2);
    // p2p2Ifaces.GetAddress(0) = 10.0.12.1 (internal2)
    // p2p2Ifaces.GetAddress(1) = 10.0.12.2 (central)

    // =========================================================
    // Populate routing tables via global routing.
    // This handles all routes between the P2P subnets and the
    // CSMA subnets that were assigned above (10.0.1.1 etc).
    // It does NOT know about the real VM addresses (10.0.N.10)
    // since those are outside ns3 — handled by static routes below.
    // =========================================================
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // =========================================================
    // Static routes for real VM subnets.
    //
    // Each internal node needs a route sending VM traffic out its
    // CSMA interface (index 1) toward the ghost/tap/VM.
    //
    // The central node needs a route for each VM subnet via the
    // corresponding internal node as next hop.
    //
    // Interface indices:
    //   internal nodes:  0=loopback  1=CSMA  2=P2P
    //   central node:    0=loopback  1=P2P-to-internal0
    //                                2=P2P-to-internal1
    //                                3=P2P-to-internal2
    // =========================================================
    Ipv4StaticRoutingHelper staticRouting;

    // --- internal0: route 10.0.1.0/24 out CSMA (iface 1) ---
    staticRouting.GetStaticRouting(internal0->GetObject<Ipv4>())
        ->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", 1);

    // --- internal1: route 10.0.2.0/24 out CSMA (iface 1) ---
    staticRouting.GetStaticRouting(internal1->GetObject<Ipv4>())
        ->AddNetworkRouteTo("10.0.2.0", "255.255.255.0", 1);

    // --- internal2: route 10.0.3.0/24 out CSMA (iface 1) ---
    staticRouting.GetStaticRouting(internal2->GetObject<Ipv4>())
        ->AddNetworkRouteTo("10.0.3.0", "255.255.255.0", 1);

    // --- central: route 10.0.1.0/24 via internal0 (10.0.10.1) on iface 1 ---
    staticRouting.GetStaticRouting(central->GetObject<Ipv4>())
        ->AddNetworkRouteTo("10.0.1.0", "255.255.255.0",
                            p2p0Ifaces.GetAddress(0), 1);

    // --- central: route 10.0.2.0/24 via internal1 (10.0.11.1) on iface 2 ---
    staticRouting.GetStaticRouting(central->GetObject<Ipv4>())
        ->AddNetworkRouteTo("10.0.2.0", "255.255.255.0",
                            p2p1Ifaces.GetAddress(0), 2);

    // --- central: route 10.0.3.0/24 via internal2 (10.0.12.1) on iface 3 ---
    staticRouting.GetStaticRouting(central->GetObject<Ipv4>())
        ->AddNetworkRouteTo("10.0.3.0", "255.255.255.0",
                            p2p2Ifaces.GetAddress(0), 3);

    // =========================================================
    // Attach TapBridges (UseBridge mode) to ghost CSMA devices
    // =========================================================
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseBridge"));

    tapBridge.SetAttribute("DeviceName", StringValue("tap0"));
    tapBridge.Install(ghost0, csma0.Get(0));

    tapBridge.SetAttribute("DeviceName", StringValue("tap1"));
    tapBridge.Install(ghost1, csma1.Get(0));

    tapBridge.SetAttribute("DeviceName", StringValue("tap2"));
    tapBridge.Install(ghost2, csma2.Get(0));

    // =========================================================
    // Run
    // =========================================================
    Simulator::Stop(Seconds(600000.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}