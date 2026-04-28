# Assignment 3: Reliable Transport Protocol (STCP)

## Overview
This assignment involves implementing STCP (Simple Transport Control Protocol), a reliable transport layer protocol built on top of the unreliable UDP protocol. The goal is to provide reliable, in-order data delivery despite the underlying network's potential for packet loss, duplication, and reordering.

## Features
- **Reliability over UDP**: Implements mechanisms to ensure data integrity and delivery over UDP.
- **Connection Setup & Teardown**: Includes a 3-way handshake for establishing connections and a systematic teardown process.
- **Flow Control**: Manages data transmission to prevent overwhelming the receiver.
- **Error Detection**: Uses checksums and sequence numbers to detect and handle corrupted or lost packets.
- **State Machine**: Implements a finite state machine (FSM) to manage different stages of a connection (LISTEN, SYN_SENT, ESTABLISHED, etc.).

## How to Build
The implementation is contained in `src/transport.c`. This file must be integrated with the STCP framework provided in the course.
1. Obtain the STCP framework (e.g., from `zip/PA3.zip`).
2. Place `transport.c` into the framework directory.
3. Install necessary dependencies:
   ```bash
   sudo apt update && sudo apt install libnsl-dev
   ```
4. Compile the project:
   ```bash
   make
   ```

## How to Run
After building, you can test the protocol using the provided server and client binaries.

### 1. Start the Server
```bash
./server
```

### 2. Run the Client
```bash
./client <server_ip>
```

## Debugging
Packet logging can be enabled by setting `LOG_PACKET` to `TRUE` in the `stcp_api.c` file within the framework.

---
*Author: SeongJi Yang (20211181)*
