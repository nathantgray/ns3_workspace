// scratch/tap-3vm-routed.cc
//
// Topology:
//   - CSMA 0: ghost0 (VM2), ghost3 (external network), internal node 0
//   - CSMA 1: ghost2 (VM1), internal node 1
//   - P2P: internal node 0 <-> central <-> internal node 1
//
// ghost0 (tap0) bridges VM2's L2 link
// ghost2 (tap2) bridges VM1's L2 link
// ghost3 (tap3) bridges the external Shared network (device 192.168.252.127)
//
// Run as root: sudo ./ns3 run tap-3vm-routed

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Tap3VmRouted");

int main(int argc, char* argv[])
{
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // =========================================================
    // Create nodes
    // =========================================================

    // Ghost nodes (hold TapBridge, no active IP role)
    Ptr<Node> ghost0 = CreateObject<Node>();  // VM2 side
    Ptr<Node> ghost2 = CreateObject<Node>();  // VM1 side
    Ptr<Node> ghost3 = CreateObject<Node>();  // External network side

    // Internal routing nodes
    Ptr<Node> internalNode0 = CreateObject<Node>();  // VM2/external subnet
    Ptr<Node> internalNode1 = CreateObject<Node>();  // VM1 subnet
    Ptr<Node> central = CreateObject<Node>();

    // =========================================================
    // Install IP stacks
    // =========================================================
    InternetStackHelper stack;
    stack.Install(ghost0);
    stack.Install(ghost2);
    stack.Install(ghost3);
    stack.Install(internalNode0);
    stack.Install(internalNode1);
    stack.Install(central);

    // Enable forwarding on routing nodes
    internalNode0->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    internalNode1->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    central->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));

    // =========================================================
    // CSMA channel 0: ghost0 (VM2) + ghost3 (external) + internalNode0
    // This is the shared segment for both VM2 and the device
    // =========================================================
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));

    NodeContainer csma0Nodes;
    csma0Nodes.Add(ghost0);
    csma0Nodes.Add(ghost3);
    csma0Nodes.Add(internalNode0);
    NetDeviceContainer csma0Devs = csma.Install(csma0Nodes);
    // csma0Devs: [0]=ghost0, [1]=ghost3, [2]=internalNode0

    // =========================================================
    // CSMA channel 1: ghost2 (VM1) + internalNode1
    // =========================================================
    NodeContainer csma1Nodes;
    csma1Nodes.Add(ghost2);
    csma1Nodes.Add(internalNode1);
    NetDeviceContainer csma1Devs = csma.Install(csma1Nodes);
    // csma1Devs: [0]=ghost2, [1]=internalNode1

    // =========================================================
    // Point-to-point: internalNode0 <-> central <-> internalNode1
    // =========================================================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2p0 = p2p.Install(internalNode0, central);
    NetDeviceContainer p2p1 = p2p.Install(internalNode1, central);

    // =========================================================
    // Assign IP addresses
    // =========================================================
    Ipv4AddressHelper address;

    // CSMA 0: 10.0.1.0/24
    // ghost0=.1, ghost3=.3, internalNode0=.2
    address.SetBase("10.0.1.0", "255.255.255.0", "0.0.0.1");
    address.Assign(csma0Devs);

    // CSMA 1: 10.0.3.0/24
    // ghost2=.1, internalNode1=.2
    address.SetBase("10.0.3.0", "255.255.255.0", "0.0.0.1");
    address.Assign(csma1Devs);

    // P2P links
    address.SetBase("10.0.10.0", "255.255.255.252", "0.0.0.1");
    address.Assign(p2p0);

    address.SetBase("10.0.11.0", "255.255.255.252", "0.0.0.1");
    address.Assign(p2p1);

    // =========================================================
    // Populate routing tables
    // =========================================================
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // =========================================================
    // TapBridges
    // =========================================================
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseBridge"));

    tapBridge.SetAttribute("DeviceName", StringValue("tap0"));
    tapBridge.Install(ghost0, csma0Devs.Get(0));

    tapBridge.SetAttribute("DeviceName", StringValue("tap3"));
    tapBridge.Install(ghost3, csma0Devs.Get(1));

    tapBridge.SetAttribute("DeviceName", StringValue("tap2"));
    tapBridge.Install(ghost2, csma1Devs.Get(0));

    // =========================================================
    // Run
    // =========================================================
    Simulator::Stop(Seconds(600000.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}