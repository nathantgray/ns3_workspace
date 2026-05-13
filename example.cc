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

// Helper that flushes immediately so output is never buffered
#define DEBUG(msg)                                   \
    do                                               \
    {                                                \
        std::cerr << "[DEBUG] " << msg << std::endl; \
        std::cerr.flush();                           \
    } while (0)

int main(int argc, char *argv[])
{
    DEBUG("main() entered — binary is running");
    DEBUG("PID = " << getpid());

    bool enableFlowMonitor = true;
    bool verbose = true;

    DEBUG("Parsing command line");
    CommandLine cmd;
    cmd.Parse(argc, argv);

    if (verbose)
    {
        DEBUG("Enabling UDP echo log components");
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("TapBridge", LOG_LEVEL_ALL);
        LogComponentEnable("TapBridgeHelper", LOG_LEVEL_ALL);
    }

    DEBUG("Binding SimulatorImplementationType to RealtimeSimulatorImpl");
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::RealtimeSimulatorImpl"));

    DEBUG("Binding ChecksumEnabled");
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    DEBUG("Creating 3 nodes");
    NodeContainer nodes;
    nodes.Create(3);
    DEBUG("Nodes created. Count = " << nodes.GetN());

    DEBUG("Setting up CSMA channel");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(500000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.1)));
    NetDeviceContainer devices = csma.Install(nodes);
    DEBUG("CSMA devices installed. Count = " << devices.GetN());

    DEBUG("Installing internet stack");
    InternetStackHelper internet;
    internet.Install(nodes);
    DEBUG("Internet stack installed");

    DEBUG("Assigning IP addresses from 192.168.4.0/24");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    DEBUG("IP addresses assigned");

    for (int i = 0; i < 3; i++)
    {
        std::cerr << "[DEBUG] Node " << i << " IP = ";
        interfaces.GetAddress(i).Print(std::cerr);
        std::cerr << std::endl;
        std::cerr.flush();
        NS_LOG_UNCOND("Node " << i << " IP = " << interfaces.GetAddress(i));
    }

    DEBUG("Installing TapBridge on node 0 with tap1");
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseLocal"));
    tapBridge.SetAttribute("DeviceName", StringValue("tap1"));
    tapBridge.Install(nodes.Get(0), devices.Get(0));
    DEBUG("TapBridge installed on node 0 / tap1");

    DEBUG("Installing TapBridge on node 1 with tap2");
    tapBridge.SetAttribute("DeviceName", StringValue("tap2"));
    tapBridge.Install(nodes.Get(1), devices.Get(1));
    DEBUG("TapBridge installed on node 1 / tap2");

    DEBUG("Installing TapBridge on node 2 with tap3");
    tapBridge.SetAttribute("DeviceName", StringValue("tap3"));
    tapBridge.Install(nodes.Get(2), devices.Get(2));
    DEBUG("TapBridge installed on node 2 / tap3");

    DEBUG("Setting up flow monitor");
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor;
    if (enableFlowMonitor)
    {
        flowMonitor = flowmonHelper.InstallAll();
        DEBUG("Flow monitor installed");
    }

    DEBUG("Enabling PCAP traces");
    csma.EnablePcap("server_0", devices.Get(0));
    csma.EnablePcap("server_1", devices.Get(1));
    csma.EnablePcap("server_2", devices.Get(2));
    DEBUG("PCAP traces enabled");

    DEBUG("Setting simulator stop time to 60000s");
    Simulator::Stop(Seconds(60000.0));

    DEBUG("Calling Simulator::Run() — simulation starts now");
    std::cerr.flush();
    Simulator::Run();

    DEBUG("Simulator::Run() returned — simulation finished or was stopped");

    if (enableFlowMonitor)
    {
        DEBUG("Serializing flow monitor to NSF.flowmon");
        flowMonitor->SerializeToXmlFile("NSF.flowmon", false, false);
        DEBUG("Flow monitor serialized");
    }

    DEBUG("Calling Simulator::Destroy()");
    Simulator::Destroy();
    DEBUG("Done — exiting main()");

    return 0;
}