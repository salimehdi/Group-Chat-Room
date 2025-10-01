# Low-Latency Chat System Prototype

This project is a simple C++ prototype of a low-latency chat/messaging system designed to demonstrate specific skills: C++ (STL), low-latency network programming (TCP, UDP, multicast, epoll), lock-free data structures, and test automation with Python and Shell scripting.

## Features

- **C++ Core Server**:
  - Handles multiple clients concurrently.
  - Supports two modes: high-performance `epoll` and baseline `thread-per-connection`.
  - Forwards messages to all connected TCP clients.
  - Broadcasts messages via UDP multicast for ultra-low latency.
  - Uses a Single-Producer, Single-Consumer (SPSC) lock-free queue for message passing between the I/O and broadcaster threads.
- **Python Client Bots**:
  - A script to simulate a configurable number of clients.
  - Clients can connect via TCP to send/receive messages or subscribe to the multicast group.
  - Measures and reports round-trip time (RTT) for TCP and one-way latency for UDP multicast.
- **Benchmarking Scripts**:
  - `start_clients.sh`: Easily launch a large number of client bots for load testing.
  - `benchmark.sh`: An automated script to build the server, run different test scenarios, and aggregate latency/throughput results into a CSV file.

## How to Build and Run

### 1. Build the Server

You will need `g++` (with C++17 support) and `cmake`.

```sh
# Create a build directory
mkdir build && cd build

# Generate Makefiles
cmake ..

# Compile server and client
make