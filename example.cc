//
// This is an illustration of how one could use virtualization techniques to
// allow running applications on virtual machines talking over simulated
// networks.
//
// The actual steps required to configure the virtual machines can be rather
// involved, so we don't go into that here. Please have a look at one of
// our HOWTOs on the nsnam wiki for more details about how to get the
// system confgured. For an example, have a look at "HOWTO Use Linux
// Containers to set up virtual networks" which uses this code as an
// example.
//
// The configuration you are after is explained in great detail in the
// HOWTO, but looks like the following:
//
//  +----------+             +----------+
//  | virtual  |             | virtual  |
//  |  Linux   |             |  Linux   |
//  |  Host    |             |  Host    |
//  |          |             |          |
//  |  eth0    |             |  eth0    |
//  +----------+             +----------+
//        |                       |
//  +----------+             +----------+
//  |  Linux   |             |  Linux   |
//  |  Bridge  |             |  Bridge  |
//  +----------+             +----------+
//        |                       |
// +------------+          +-------------+
// | "tap-left" |          | "tap-right" |
// +------------+          +-------------+
//       |    n0            n1    |
//       | +--------+  +--------+ |
//       +-| tap    |  | tap    |-+
//         | bridge |  | bridge |
//         +--------+  +--------+
//         | CSMA   |  | CSMA   |
//         +--------+  +--------+
//              |           |
//              |           |
//              |           |
//             ===============
//                CSMA LAN
//

#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TapCsmaVirtualMachineExample");

int
main (int argc, char *argv[])
{
    bool enableFlowMonitor = true;
    bool verbose = true;
    CommandLine cmd;
    cmd.Parse (argc, argv);
    if (verbose)
    {
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    //
    // We are interacting with the outside, real, world. This means we have to
    // interact in real-time and therefore means we have to use the real-time
    // simulator and take the time to calculate checksums.
    //
    std::string anim_name ("NSF.anim.xml");

    GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    //
    // Create two ghost nodes. The first will represent the virtual machine host
    // on the left side of the network; and the second will represent the VM on
    // the right side.
    //
    NodeContainer nodes;
    nodes.Create (5);

    //
    // Use a CsmaHelper to get a CSMA channel created, and the needed net
    // devices installed on both of the nodes. The data rate and delay for the
    // channel can be set through the command-line parser. For example,
    //
    // ./waf --run "tap=csma-virtual-machine --ns3::CsmaChannel::DataRate=10000000"
    //
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", DataRateValue (500000000));
    //csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    //5000000
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (.1)));
    NetDeviceContainer devices = csma.Install (nodes);

    InternetStackHelper internet;
    internet.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("192.168.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
    for (int i=0;i<5;i++)
    {NS_LOG_UNCOND (interfaces.GetAddress (i));}

    //
    // Use the TapBridgeHelper to connect to the pre-configured tap devices for
    // the left side. We go with "UseBridge" mode since the CSMA devices support
    // promiscuous mode and can therefore make it appear that the bridge is
    // extended into ns-3. The install method essentially bridges the specified
    // tap to the specified CSMA device.
    //
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute ("Mode", StringValue ("UseBridge"));
    tapBridge.SetAttribute ("DeviceName", StringValue ("mytap1"));
    tapBridge.Install (nodes.Get (1), devices.Get (0));

    //
    // Connect the right side tap to the right side CSMA device on the right-side
    // ghost node.
    //
    tapBridge.SetAttribute ("DeviceName", StringValue ("mytap2"));
    tapBridge.Install (nodes.Get (4), devices.Get (4));

    /*
    uint16_t port = 9; // Discard port (RFC 863)
    OnOffHelper onoff ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
    onoff.SetConstantRate (DataRate ("500Mb/s"));

    ApplicationContainer apps = onoff.Install (nodes.Get (1));
    apps.Start (Seconds (10.0));
    apps.Stop (Seconds (20.0));

    // Create a packet sink to receive these packets
    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (4), port));

    apps = sink.Install (nodes.Get (4));
    apps.Start (Seconds (10.0));
    apps.Stop (Seconds (20.0));

    UdpEchoServerHelper echoServer (10);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (60.0));

    Ipv4Address ServerAddress(interfaces.GetAddress (3));
    uint16_t Server_port = 10;

    UdpEchoClientHelper echoClient (ServerAddress, Server_port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (20000));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (10024));

    ApplicationContainer clientApps =
    echoClient.Install (nodes.Get (1));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (60.0));
    */

    //
    // Run the simulation for ten minutes to give the user time to play around
    //
    /*
    AnimationInterface anim (anim_name.c_str ());
    //NS_LOG_INFO ("Run Simulation.");
    anim.UpdateNodeDescription(nodes.Get(0),"IED");
    anim.UpdateNodeColor(nodes.Get(0),0,128,0);
    anim.UpdateNodeSize(0,2.0,2.0);

    anim.UpdateNodeDescription(nodes.Get(1),"IED");
    anim.UpdateNodeColor(nodes.Get(1),0,128,0);
    anim.UpdateNodeSize(1,2.0,2.0);

    anim.UpdateNodeDescription(nodes.Get(2),"IED");
    anim.UpdateNodeColor(nodes.Get(2),0,128,0);
    anim.UpdateNodeSize(2,2.0,2.0);

    anim.UpdateNodeDescription(nodes.Get(3),"IED");
    anim.UpdateNodeColor(nodes.Get(3),0,128,0);
    anim.UpdateNodeSize(3,2.0,2.0);

    anim.UpdateNodeDescription(nodes.Get(4),"IED");
    anim.UpdateNodeColor(nodes.Get(4),0,128,0);
    anim.UpdateNodeSize(4,2.0,2.0);
    */

    FlowMonitorHelper flowmonHelper;
    if (enableFlowMonitor)
    {
        flowmonHelper.InstallAll ();
    }

    if (enableFlowMonitor)
    {
        flowmonHelper.SerializeToXmlFile ("NSF.flowmon", false, false);
    }

    csma.EnablePcap("server_4",devices.Get (4));
    csma.EnablePcap("server_0",devices.Get (0));
    Simulator::Stop (Seconds (60000.0));
    Simulator::Run ();
    Simulator::Destroy ();
}

/*
Bash Script:
```bash
sudo tunctl -t mytap1
sudo ifconfig mytap1 0.0.0.0 promisc up
sudo tunctl -t mytap2
sudo ifconfig mytap2 0.0.0.0 promisc up
sudo brctl addbr mybridge
sudo brctl addif mybridge mytap1
sudo brctl addif mybridge enp3s0
sudo brctl addbr yourbridge
sudo brctl addif yourbridge mytap2
sudo brctl addif yourbridge enx7cc2c6331c3c
sudo ifconfig yourbridge up
sudo ifconfig mybridge up
```
*/