1) run `sudo bash scripts/setup-bridges.sh` to create the TAP bridges and enslave them to the physical NICs
2) build tap-3vm-routed by copying to the scratch directory and running `./ns3 build tap-3vm-routed`
3) run the simulation with `sudo ./ns3 run tap-3vm-routed`

# Network Setup
- 3 VMs are each connected to the central ns3-router VM via L2 links.
- 
ip -brief link show
sudo ip addr add 10.0.1.10/24 dev ens4
sudo ip link set ens4 up
sudo ip route add default via 10.0.1.2 dev ens4
# Setup on Linux VMs

# Setup on Windows VM
1) Add ip address to NIC connected to ns3-router 10.0.2.10/24 with default gateway 10.0.2.2. However this default gateway will likely be overridden by the higher priority default gateway for the other NIC, so you may need to add a static route for each destination IP.
    a) check routes with `route print -4`
    b) add static routes with `route add 10.0.1.0 mask 255.255.255.0 10.0.2.2 -p` and `route add 10.0.3.0 mask 255.255.255.0 10.0.2.2 -p`
