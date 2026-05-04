from ns import ns

ns.GlobalValue.Bind("SimulatorImplementationType",
                     ns.StringValue("ns3::RealtimeSimulatorImpl"))
ns.GlobalValue.Bind("ChecksumEnabled", ns.BooleanValue(True))

nodes = ns.NodeContainer()
nodes.Create(4)

csma = ns.CsmaHelper()
csma.SetChannelAttribute("DataRate", ns.DataRateValue(ns.DataRate(5000000)))
csma.SetChannelAttribute("Delay", ns.TimeValue(ns.MilliSeconds(5)))
devices = csma.Install(nodes)

stack = ns.InternetStackHelper()
stack.Install(nodes)

address = ns.Ipv4AddressHelper()
address.SetBase(ns.Ipv4Address("10.1.1.0"), ns.Ipv4Mask("255.255.255.0"))
interfaces = address.Assign(devices)

tap = ns.TapBridgeHelper()
tap.SetAttribute("Mode", ns.StringValue("ConfigureLocal"))
tap.SetAttribute("DeviceName", ns.StringValue("tap-ns3"))
tap.Install(nodes.Get(0), devices.Get(0))

# This was missing — needed for routing between nodes
ns.Ipv4GlobalRoutingHelper.PopulateRoutingTables()

print("Simulation running — ping 10.1.1.2 from another terminal")
print("Press Ctrl+C to stop")

ns.Simulator.Stop(ns.Seconds(300.0))
ns.Simulator.Run()
ns.Simulator.Destroy()