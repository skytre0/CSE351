# Assignment 4: Simple Router

## Overview
This assignment involves implementing a fully functional IP router that operates on a simulated network using Mininet and the POX controller. The router handles IP forwarding, ARP (Address Resolution Protocol) management, and ICMP (Internet Control Message Protocol) error and diagnostic messages.

## Features
- **IP Forwarding**: Implements Longest Prefix Match (LPM) logic to forward packets to the correct next-hop interface.
- **ARP Cache Management**: 
  - Maintains a dynamic ARP cache and queues packets waiting for ARP replies.
  - Sends ARP requests when a destination MAC address is unknown.
- **ICMP Handling**: Supports Echo Reply, Destination Unreachable (Net/Host/Port), and Time Exceeded (TTL).
- **Firewall (Blacklist)**: Blocks traffic from/to specific IP ranges (e.g., `10.0.2.0/24`).

## Environment Setup
This assignment must be run within the provided **Virtual Machine (VM)** environment.

### 1. Accessing the VM
Connect to the VM via SSH or use the VM console:
```bash
ssh -p 2222 cse351@127.0.0.1
# Password: 1234
```

### 2. Preparing the Source
Navigate to the project directory within the VM:
```bash
cd ~/cse351_sr
```

## How to Build
1. Move your `sr_router.c` and `sr_arpcache.c` into the `~/cse351_sr/router` directory.
2. Compile the router:
   ```bash
   cd ~/cse351_sr/router
   make
   ```
   *Note: Use `make dist-clean; make` if you encounter build errors.*

## How to Run
Execution requires three separate terminal sessions inside the VM:

### Terminal 1: Start the POX Controller
```bash
cd ~/cse351_sr
./run_pox.sh
```

### Terminal 2: Start Mininet Simulation
```bash
cd ~/cse351_sr
sudo ./run_mininet.sh
```

### Terminal 3: Run the Simple Router
```bash
cd ~/cse351_sr/router
./sr
```

## Testing and Verification
Once all components are running, use the **Mininet console** (Terminal 2) to verify functionality:
- **Success Case**: `client1 ping -c 3 172.64.3.10`
- **Firewall Test**: `client2 ping server1` (Should be blocked if 10.0.2.0/24 is blacklisted)
- **Traceroute**: `client1 traceroute -n 192.168.2.2`
- **ICMP Error**: `client1 ping -c 3 8.8.8.8` (Should return Net Unreachable)
