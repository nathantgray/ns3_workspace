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
    // Create nodes
    // =========================================================
    NodeContainer ghosts;
    ghosts.Create(3);
    NodeContainer internalNodes;
    internalNodes.Create(3);
    Ptr<Node> central = CreateObject<Node>();
    // =========================================================
    // Install IP stacks
    // =========================================================
    InternetStackHelper stack;
    stack.Install(ghosts);
    stack.Install(internalNodes);
    stack.Install(central);
    // =========================================================
    // Enable IP forwarding on internal nodes and central node
    // =========================================================
    for (uint32_t i = 0; i < 3; ++i)
    {
        internalNodes.Get(i)->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    }
    central->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    // =========================================================
    // CSMA segments: ghost <-> internal node (one per VM)
    // =========================================================
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
    NetDeviceContainer csmaDevs[3]; // each contains {ghost dev, internal dev}
    for (uint32_t i = 0; i < 3; ++i)
    {
        NodeContainer pair(ghosts.Get(i), internalNodes.Get(i));
        csmaDevs[i] = csma.Install(pair);
    }
    // =========================================================
    // Point-to-point links: internal node <-> central node
    // =========================================================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevs[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        p2pDevs[i] = p2p.Install(internalNodes.Get(i), central);
    }
    // =========================================================
    // Assign IP addresses
    //
    // Topology per segment:
    //
    //   [VM: 10.0.N.10]
    //        |  (real Linux tap/bridge)
    //   [ghost node]        <-- no IP assigned, L2 bridge only
    //        |  (CSMA)
    //   [internal node: 10.0.N.1]
    //        |  (P2P)
    //   [central node: 10.0.1N.2]
    //
    //   CSMA subnets : 10.0.1.0/24, 10.0.2.0/24, 10.0.3.0/24
    //   P2P  subnets : 10.0.10.0/30, 10.0.11.0/30, 10.0.12.0/30
    //   VM addresses : 10.0.1.10, 10.0.2.10, 10.0.3.10 (configured on real VMs)
    // =========================================================
    Ipv4AddressHelper address;

    // --- CSMA subnets ---
    // Ghost nodes act as a pure L2 bridge for the TapBridge.
    // Assigning an IP to the ghost would cause it to respond to ARP
    // and conflict with the real VM on the same subnet, so we assign
    // only to the internal node side of each CSMA link.
    const char *csmaSubnets[3] = {"10.0.1.0", "10.0.2.0", "10.0.3.0"};
    Ipv4InterfaceContainer csmaInternalIfaces[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        address.SetBase(Ipv4Address(csmaSubnets[i]), Ipv4Mask("255.255.255.0"));

        // csmaDevs[i].Get(0) = ghost device    — intentionally skipped
        // csmaDevs[i].Get(1) = internal device — gets 10.0.N.1
        NetDeviceContainer internalCsmaDev;
        internalCsmaDev.Add(csmaDevs[i].Get(1));
        csmaInternalIfaces[i] = address.Assign(internalCsmaDev);
    }

    // --- P2P subnets ---
    // /30 gives exactly two host addresses per link:
    //   .1 = internal node side
    //   .2 = central node side
    const char *p2pSubnets[3] = {"10.0.10.0", "10.0.11.0", "10.0.12.0"};
    Ipv4InterfaceContainer p2pInternalIfaces[3];
    Ipv4InterfaceContainer p2pCentralIfaces[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        address.SetBase(Ipv4Address(p2pSubnets[i]), Ipv4Mask("255.255.255.252"));

        // p2pDevs[i].Get(0) = internal node — gets 10.0.1N.1
        // p2pDevs[i].Get(1) = central node  — gets 10.0.1N.2
        Ipv4InterfaceContainer ifaces = address.Assign(p2pDevs[i]);
        p2pInternalIfaces[i].Add(ifaces.Get(0));
        p2pCentralIfaces[i].Add(ifaces.Get(1));
    }

    // =========================================================
    // Populate routing tables via global routing.
    // GlobalRouting sees the P2P and CSMA subnets that were
    // assigned above and builds routes between internal nodes
    // and the central node automatically.
    // =========================================================
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // =========================================================
    // Add static routes for VM subnets.
    //
    // GlobalRouting only knows about ns3-assigned addresses.
    // The real VMs live at e.g. 10.0.1.10 which ns3 never sees,
    // so we add an explicit route on each internal node directing
    // any traffic for its CSMA subnet out through its CSMA interface.
    //
    // We also add routes on the central node so it can reach each
    // VM subnet via the correct internal node.
    // =========================================================
    Ipv4StaticRoutingHelper staticRouting;

    for (uint32_t i = 0; i < 3; ++i)
    {
        // -- Internal node i --
        // Interface indices on an internal node:
        //   0 = loopback
        //   1 = CSMA  (toward ghost / VM)
        //   2 = P2P   (toward central)
        Ptr<Ipv4StaticRouting> internalSR =
            staticRouting.GetStaticRouting(internalNodes.Get(i)->GetObject<Ipv4>());

        // Forward VM-subnet traffic out the CSMA interface (index 1)
        internalSR->AddNetworkRouteTo(
            Ipv4Address(csmaSubnets[i]),
            Ipv4Mask("255.255.255.0"),
            1); // CSMA interface index

        // -- Central node --
        // Route each VM subnet via the corresponding internal node's P2P address
        Ptr<Ipv4StaticRouting> centralSR =
            staticRouting.GetStaticRouting(central->GetObject<Ipv4>());

        // Next hop = internal node's P2P address (.1 side)
        Ipv4Address nextHop = p2pInternalIfaces[i].GetAddress(0);
        centralSR->AddNetworkRouteTo(
            Ipv4Address(csmaSubnets[i]),
            Ipv4Mask("255.255.255.0"),
            nextHop,
            i + 1); // P2P interface index on central (1-based, one per internal node)
    }

    // =========================================================
    // Attach TapBridges (UseBridge mode) to ghost CSMA devices
    // =========================================================
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseBridge"));
    const char *tapNames[3] = {"tap0", "tap1", "tap2"};
    for (uint32_t i = 0; i < 3; ++i)
    {
        tapBridge.SetAttribute("DeviceName", StringValue(tapNames[i]));
        tapBridge.Install(ghosts.Get(i), csmaDevs[i].Get(0));
    }
    // =========================================================
    // Run
    // =========================================================
    Simulator::Stop(Seconds(600000.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}