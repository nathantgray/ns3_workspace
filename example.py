from ns import ns
import sys


def main(argv):
    enable_flow_monitor = True
    verbose = True

    cmd = ns.CommandLine()
    cmd.Parse(argv)

    if verbose:
        ns.LogComponentEnable("UdpEchoClientApplication", ns.LOG_LEVEL_INFO)
        ns.LogComponentEnable("UdpEchoServerApplication", ns.LOG_LEVEL_INFO)

    ns.GlobalValue.Bind(
        "SimulatorImplementationType",
        ns.StringValue("ns3::RealtimeSimulatorImpl"),
    )
    ns.GlobalValue.Bind("ChecksumEnabled", ns.BooleanValue(True))

    nodes = ns.NodeContainer()
    nodes.Create(3)

    csma = ns.CsmaHelper()
    csma.SetChannelAttribute("DataRate", ns.DataRateValue(ns.DataRate(500000000)))
    csma.SetChannelAttribute("Delay", ns.TimeValue(ns.MilliSeconds(0.1)))
    devices = csma.Install(nodes)

    internet = ns.InternetStackHelper()
    internet.Install(nodes)

    ipv4 = ns.Ipv4AddressHelper()
    ipv4.SetBase(ns.Ipv4Address("192.168.4.0"), ns.Ipv4Mask("255.255.255.0"))
    interfaces = ipv4.Assign(devices)

    for i in range(nodes.GetN()):
        print(interfaces.GetAddress(i))

    tap_bridge = ns.TapBridgeHelper()
    tap_bridge.SetAttribute("Mode", ns.StringValue("UseBridge"))
    tap_bridge.SetAttribute("DeviceName", ns.StringValue("tap1"))
    tap_bridge.Install(nodes.Get(0), devices.Get(0))

    tap_bridge.SetAttribute("DeviceName", ns.StringValue("tap2"))
    tap_bridge.Install(nodes.Get(1), devices.Get(1))

    tap_bridge.SetAttribute("DeviceName", ns.StringValue("tap3"))
    tap_bridge.Install(nodes.Get(2), devices.Get(2))

    flowmon_helper = ns.FlowMonitorHelper()
    if enable_flow_monitor:
        flowmon_helper.InstallAll()
        flowmon_helper.SerializeToXmlFile("NSF.flowmon", False, False)

    csma.EnablePcap("server_0", devices.Get(0))
    csma.EnablePcap("server_1", devices.Get(1))
    csma.EnablePcap("server_2", devices.Get(2))

    ns.Simulator.Stop(ns.Seconds(60000.0))
    ns.Simulator.Run()
    ns.Simulator.Destroy()

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
