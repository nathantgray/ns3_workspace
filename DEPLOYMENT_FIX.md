# TAP Creator PATH Fix - Deployment Instructions

## Problem
On servers, the simulation fails with:
```
OSError: [Errno 2] No such file or directory: 'tap-creator'
```

Even though the binary exists at `.venv/lib/python3.12/site-packages/ns3/libexec/ns3/ns3.44-tap-creator`

## Root Cause
- ns-3 tap-bridge library hardcodes lookup for `ns3.44-tap-creator` binary
- Sudo's secure_path does NOT include user .venv directories
- Therefore the sudo environment cannot find the executable

## Solution Implemented
Updated `scripts/run_example.sh` to:
1. Define `NS3_LIBEXEC_DIR` pointing to the ns3 libexec directory
2. Validate it exists
3. **Prepend it to PATH** when running Python:
   ```bash
   PATH="$NS3_LIBEXEC_DIR${PATH:+:$PATH}"
   ```

## Testing Status
✅ Tested on laptop:
- Script runs without tap-creator errors
- Simulation starts and runs for extended periods
- Ready for server deployment

## Deployment Steps
1. Clone/sync the updated code to the server
2. Verify `.venv` is initialized on the server with same ns-3 version
3. Run:
   ```bash
   sudo /home/ubuntu/ns3_workspace/scripts/run_example.sh
   ```
   (Adjust path as needed for your server)

## Verification
If successful, you will see:
```
Done.
tap1 <-> br-tap1 <-> ens4
tap2 <-> br-tap2 <-> ens5
tap3 <-> br-tap3 <-> ens6
Starting example.py...
```

Then the simulation will begin running.

## Alternative: Manual TAP/Bridge Setup
If you need to set up TAP/bridge infrastructure without running the simulation:
```bash
sudo ./scripts/setup_tap_bridges.sh
```

To clean up afterward:
```bash
sudo ./scripts/teardown_tap_bridges.sh
```

## Files Modified
- `scripts/run_example.sh` - Added NS3_LIBEXEC_DIR and PATH export

## Architecture
```
┌─────────────────────────────────────────┐
│  example.py (ns-3 simulation)           │
│  - 3 nodes (CSMA LAN)                   │
│  - TapBridge (tap1/2/3)                 │
│  - FlowMonitor + PCAP capture           │
└──────┬────────────────────────────────┬─┘
       │                                │
   ┌───▼─────────────┐          ┌──────▼──────────┐
   │ Linux Bridge    │          │ Linux Bridge    │
   │ (br-tap1/2/3)   │          │ (br-tap3)       │
   │ + TAP device    │          │ + TAP device    │
   │ + PHY NIC       │          │ + PHY NIC       │
   │ (ens4/5/6)      │          │ (ens6)          │
   └────────────────┘          └─────────────────┘
         │                            │
    [External Server]         [External Server]
    192.168.4.1               192.168.4.3
```

## Next Steps After Deployment
1. Verify all 3 TAP bridges are created and enslaved
2. Configure static IPs on external servers (192.168.4.1/3/5)
3. Add static routes on external servers to force traffic through ns3-router
4. Test connectivity across the simulated network
