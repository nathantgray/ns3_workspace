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
    // Install IP stacks on ALL nodes (including ghosts)
    // Ghost nodes need the stack for global routing to discover
    // their subnets and for ARP to function properly.
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
    // =========================================================
    Ipv4AddressHelper address;

    // CSMA subnets — assign to BOTH ghost and internal node.
    // Ghost gets .1, internal node gets .2.
    // VMs will use .10 (or whatever you configure on the real VM).
    // The ghost's .1 address won't conflict because the TapBridge
    // operates at L2 and the ghost's IP stack won't actively use it
    // for traffic — it just makes global routing aware of the subnet.
    const char *csmaSubnets[3] = {"10.0.1.0", "10.0.2.0", "10.0.3.0"};

    for (uint32_t i = 0; i < 3; ++i)
    {
        address.SetBase(Ipv4Address(csmaSubnets[i]),
                        Ipv4Mask("255.255.255.0"),
                        Ipv4Address("0.0.0.1"));
        address.Assign(csmaDevs[i]); // .1 = ghost, .2 = internal node
    }

    // P2P subnets between internal nodes and central
    const char *p2pSubnets[3] = {"10.0.10.0", "10.0.11.0", "10.0.12.0"};

    for (uint32_t i = 0; i < 3; ++i)
    {
        address.SetBase(Ipv4Address(p2pSubnets[i]),
                        Ipv4Mask("255.255.255.252"),
                        Ipv4Address("0.0.0.1"));
        address.Assign(p2pDevs[i]);
    }

    // =========================================================
    // Populate routing tables
    // =========================================================
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

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