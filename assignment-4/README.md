# Assignment 4: Simple Router

## Overview
This assignment involves implementing a fully functional IP router that operates on a simulated network using Mininet and the POX controller. The router handles IP forwarding, ARP (Address Resolution Protocol) management, and ICMP (Internet Control Message Protocol) error and diagnostic messages.

## Features
- **IP Forwarding**: Implements Longest Prefix Match (LPM) logic to forward packets to the correct next-hop interface.
- **ARP Cache Management**: 
  - Maintains a dynamic ARP cache.
  - Queues packets waiting for ARP replies.
  - Sends ARP requests when a destination MAC address is unknown.
- **ICMP Handling**: 
  - **Echo Reply (Type 0)**: Responds to pings directed at the router's own interfaces.
  - **Destination Unreachable (Type 3)**: Handles Net Unreachable (Code 0), Host Unreachable (Code 1), and Port Unreachable (Code 3).
  - **Time Exceeded (Type 11)**: Sends when an IP packet's TTL reaches 0.
- **Firewall (Blacklist)**: Implements a basic security feature to block traffic from or to specific IP ranges (e.g., `10.0.2.0/24`).

## How to Build
The core logic resides in `src/sr_router.c` and `src/sr_arpcache.c`. These files are part of the `sr` (Simple Router) skeleton.
1. Integrate the source files into the router framework.
2. Compile the router:
   ```bash
   make
   ```
   *If build fails, try `make dist-clean; make`.*

## How to Run
The environment requires three terminal windows (or sessions):

### 1. Terminal 1: POX Controller
```bash
./run_pox.sh
```

### 2. Terminal 2: Mininet Simulation
```bash
sudo ./run_mininet.sh
```

### 3. Terminal 3: Router Execution
```bash
cd router
./sr
```

## Testing and Verification
Once the router is running, use the Mininet console to run tests:
- **Ping Test**: `client1 ping -c 3 172.64.3.10`
- **Traceroute**: `client1 traceroute -n 192.168.2.2`
- **Firewall Check**: `client2 ping server1` (Verification of blacklisted IP blocking).

---
*Author: SeongJi Yang (20211181)*
