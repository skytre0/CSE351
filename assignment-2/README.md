# Assignment 2: HTTP Proxy Server

## Overview
This assignment involves implementing a simple HTTP/1.0 proxy server. The proxy server facilitates communication between a client and a web server by forwarding requests and responses. It ensures that requests are valid according to the HTTP/1.0 specification before processing them.

## Features
- **HTTP/1.0 Protocol**: Implements the fundamental aspects of the HTTP/1.0 protocol.
- **GET Method Support**: Handles standard `GET` requests from clients.
- **Header Validation**: Validates the `Host` header and ensures it matches the requested URL.
- **Robust Error Handling**: Provides appropriate HTTP error responses (e.g., `400 Bad Request`) for malformed or invalid requests.
- **Sequential Handling**: Processes transactions and closes connections after the data transfer is complete.

## How to Build
A `Makefile` is included in the `src` directory.
```bash
cd src
make
```

## How to Run

### 1. Start the Proxy Server
Run the proxy server by specifying a listening port.
```bash
./proxy <port_number>
```
Example: `./proxy 5678`

### 2. Test the Proxy
You can verify the proxy functionality using `curl` or `telnet`.

**Using curl:**
```bash
curl --proxy http://localhost:5678 http://www.google.com/
```

**Using telnet:**
Connect to the proxy and manually type the HTTP request:
```bash
telnet localhost 5678
GET http://www.google.com/ HTTP/1.0
Host: www.google.com
(Enter an empty line to finish the request)
```

## Environment
- This project is designed to run in a Linux environment.
- Docker configuration details can be found in `docs/컴네 assignment 2.txt`.
