#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TapCsmaVirtualMachineExample");

int main(int argc, char *argv[])
{
    bool enableFlowMonitor = true;
    bool verbose = true;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Create 3 nodes to match example.py
    NodeContainer nodes;
    nodes.Create(3);

    // CSMA channel matching example.py settings
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(500000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.1)));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Same subnet as example.py
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    for (int i = 0; i < 3; i++)
    {
        NS_LOG_UNCOND(interfaces.GetAddress(i));
    }

    // Connect tap1, tap2, tap3 to match run_example.sh and example.py
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseBridge"));

    tapBridge.SetAttribute("DeviceName", StringValue("tap1"));
    tapBridge.Install(nodes.Get(0), devices.Get(0));

    tapBridge.SetAttribute("DeviceName", StringValue("tap2"));
    tapBridge.Install(nodes.Get(1), devices.Get(1));

    tapBridge.SetAttribute("DeviceName", StringValue("tap3"));
    tapBridge.Install(nodes.Get(2), devices.Get(2));

    // Flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor;
    if (enableFlowMonitor)
    {
        flowMonitor = flowmonHelper.InstallAll();
    }

    // PCAP traces for all 3 nodes to match example.py
    csma.EnablePcap("server_0", devices.Get(0));
    csma.EnablePcap("server_1", devices.Get(1));
    csma.EnablePcap("server_2", devices.Get(2));

    Simulator::Stop(Seconds(60000.0));
    Simulator::Run();

    // Serialize flow monitor AFTER Run so it contains actual flow data
    if (enableFlowMonitor)
    {
        flowMonitor->SerializeToXmlFile("NSF.flowmon", false, false);
    }

    Simulator::Destroy();

    return 0;
}