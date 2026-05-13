// scratch/tap-3vm-l2.cc
//
// L2-bridge three real VMs (VM1/VM2/VM3) into a single ns-3 CSMA segment
// via TapBridge UseBridge mode. Each of the ns3-router host's real NICs
// (ens4/ens5/ens6) has been pre-joined with a tap (tap0/tap1/tap2) inside
// an OS bridge (br0/br1/br2) by the host-side setup script.
//
// Run as root:
//   sudo ./ns3 run tap-3vm-l2

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Tap3VmL2");

int main(int argc, char *argv[])
{
    // Emulation requires real-time scheduling and checksum computation.
    GlobalValue::Bind("SimulatorImplementationType",
                      StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Three "ghost" nodes, one per bridged VM. They hold only the TapBridge
    // and its bridged CSMA device; no IP stack is needed for pure L2.
    NodeContainer ghosts;
    ghosts.Create(3);

    // Single shared CSMA channel -> one L2 broadcast domain for all 3 VMs.
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    // No delay, no loss — simplest baseline.

    NetDeviceContainer devs = csma.Install(ghosts);

    // Attach a TapBridge in UseBridge mode to each ghost's CSMA device.
    // DeviceName is the pre-created tap on the host; the host has already
    // placed that tap in a bridge together with the real NIC facing the VM.
    TapBridgeHelper tap;
    tap.SetAttribute("Mode", StringValue("UseBridge"));

    tap.SetAttribute("DeviceName", StringValue("tap0"));
    tap.Install(ghosts.Get(0), devs.Get(0));

    tap.SetAttribute("DeviceName", StringValue("tap1"));
    tap.Install(ghosts.Get(1), devs.Get(1));

    tap.SetAttribute("DeviceName", StringValue("tap2"));
    tap.Install(ghosts.Get(2), devs.Get(2));

    // Optional: pcap on the CSMA side for debugging.
    // csma.EnablePcapAll("tap-3vm-l2", true);

    Simulator::Stop(Seconds(600000.0)); // run for 10 minutes; adjust as needed
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}