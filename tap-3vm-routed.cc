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

    NetDeviceContainer csmaDevs[3];
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
    // CSMA subnets:
    //   subnet.1  — ghost node (needed so TapBridge can init the device)
    //   subnet.2  — internal node
    //   subnet.10 — real VM (configured on the VM itself)
    //
    // VM1 (tap1/ghost[1]): 192.168.252.0/22, with ns3-assigned
    // addresses pinned into 192.168.254.x
    // VM0/VM2:             10.0.{1,3}.0/24
    //
    // The ghost IP does not cause practical ARP conflicts because
    // the TapBridge at L2 means the ghost IP stack is never
    // actively used for traffic.
    // =========================================================
    Ipv4AddressHelper address;

    const char *csmaSubnets[3] = {"10.0.1.0", "192.168.252.0", "10.0.3.0"};
    const char *csmaMasks[3] = {"255.255.255.0", "255.255.252.0", "255.255.255.0"};
    const char *csmaBases[3] = {"0.0.0.1", "0.0.2.1", "0.0.0.1"};
    for (uint32_t i = 0; i < 3; ++i)
    {
        address.SetBase(Ipv4Address(csmaSubnets[i]), Ipv4Mask(csmaMasks[i]), Ipv4Address(csmaBases[i]));

        // Assign to both ghost (.1) and internal node (.2)
        address.Assign(csmaDevs[i]);
    }

    // P2P subnets between internal nodes and central node
    const char *p2pSubnets[3] = {"10.0.10.0", "10.0.11.0", "10.0.12.0"};
    for (uint32_t i = 0; i < 3; ++i)
    {
        address.SetBase(Ipv4Address(p2pSubnets[i]), Ipv4Mask("255.255.255.252"));
        address.Assign(p2pDevs[i]);
    }

    // =========================================================
    // Populate global routing tables
    // This handles all routes between internal nodes and central.
    // =========================================================
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4GlobalRoutingHelper g;
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>("routing-tables.txt", std::ios::out);
    g.PrintRoutingTableAllAt(Seconds(1.0), routingStream);
    // =========================================================
    // Static routes on each internal node toward the VM subnet.
    //
    // GlobalRouting only knows about ns3-assigned addresses.
    // The real VMs live at e.g. 10.0.1.10, which ns3 never sees.
    // We add a catch-all route for the whole CSMA subnet so that
    // return traffic from other nodes reaches the VM via the
    // internal node's CSMA interface.
    //
    // Interface index on internalNodes:
    //   0 — loopback
    //   1 — CSMA (toward ghost / VM)
    //   2 — P2P (toward central)
    // =========================================================
    Ipv4StaticRoutingHelper staticRouting;
    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<Ipv4StaticRouting> sr =
            staticRouting.GetStaticRouting(internalNodes.Get(i)->GetObject<Ipv4>());

        sr->AddNetworkRouteTo(
            Ipv4Address(csmaSubnets[i]), // destination network
            Ipv4Mask(csmaMasks[i]),      // mask
            1                            // out via CSMA interface
        );
    }

    // =========================================================
    // Attach TapBridges to ghost CSMA devices (UseBridge mode)
    // Assumes tap0/tap1/tap2 are already part of Linux bridges.
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