# Computer Networks

This repository contains four assignments for the Computer Network course at UNIST.

## Assignments Overview

### [Assignment 1: Socket Programming (Vigenère Cipher)](./assignment-1/)
- Implementation of a Client-Server application for Vigenère encryption and decryption.
- Supports handling messages up to 10MB and handles multiple concurrent clients using `epoll` or similar mechanisms.

### [Assignment 2: HTTP Proxy Server](./assignment-2/)
- Implementation of a basic HTTP/1.0 proxy server.
- Supports `GET` requests and handles host header validation.

### [Assignment 3: Reliable Transport Protocol (STCP)](./assignment-3/)
- Implementation of a reliable transport protocol (STCP) over an unreliable UDP network.
- Features include connection management (handshake/teardown) and reliable data transfer.

### [Assignment 4: Simple Router](./assignment-4/)
- Implementation of a functional router that handles IP forwarding, ARP, and ICMP messages.
- Built to run within a Mininet environment with a POX controller.

## Directory Structure

Each assignment folder is organized as follows:
- `src/`: Source code and Makefiles.
- `report/`: Final reports in PDF/DOCX format.
- `docs/`: Assignment instructions and reference documents.
- `zip/`: Original submission files and templates.
