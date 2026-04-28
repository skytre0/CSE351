# Assignment 1: Vigenère Cipher Socket Programming

## Overview
This assignment implements a Client-Server architecture for encrypting and decrypting messages using the Vigenère cipher. The communication is performed over TCP sockets, and the server is designed to handle multiple concurrent clients efficiently using `epoll` and non-blocking I/O.

## Protocol Specification
The communication protocol uses a fixed-size header followed by variable-length data:
1. **Op Field** (16 bits): 
   - `0`: Encryption
   - `1`: Decryption
2. **Key Length** (16 bits): Length of the keyword (Network Byte Order).
3. **Data Length** (32 bits): Length of the message data (Network Byte Order).
4. **String Field**: 
   - Contains the Keyword followed immediately by the Message Data.

## Features
- **Concurrency**: The server can handle up to 50 concurrent connections.
- **Large Data Support**: Supports message sizes up to 10MB. Larger messages are split by the client.
- **Input Validation**: Strict validation of operation codes, keywords (alphabetic only), and message lengths.
- **Vigenère Cipher**: Correctly handles mixed-case alphabets while preserving non-alphabetic characters.

## How to Build
A `Makefile` is provided in the `src` directory.
```bash
cd src
make all
```

## How to Run

### 1. Start the Server
Run the server by specifying a port number.
```bash
./server -p <port_number>
```
Example: `./server -p 8888`

### 2. Run the Client
The client requires the server's IP, port, operation mode, and a keyword.
```bash
./client -h <host_ip> -p <port_number> -o <0|1> -k <keyword> < <input_file>
```
- `-o 0`: Encryption mode.
- `-o 1`: Decryption mode.

Example (Encrypting a file):
```bash
./client -h 127.0.0.1 -p 8888 -o 0 -k mysecretkey < input.txt > encrypted.txt
```

## Environment
- This project is developed for a Linux/Ubuntu environment.
- Using Docker is recommended as specified in `docs/컴네 assignment 1.txt`.
