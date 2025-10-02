# Low-Latency Group Chat Room

This project is a high-performance, low-latency group chat server and client implementation in C++. It demonstrates advanced network programming techniques, including the use of `epoll` for efficient I/O, lock-free data structures for inter-thread communication, and UDP multicast for low-latency broadcasting. The project also includes a suite of Python-based tools for load testing.

---

## Features

* **High-Performance C++ Server**: The core of the project is a C++ server that can handle a large number of concurrent clients with minimal latency.
* **Multiple Server Modes**: The server can be run in two different modes:
    * **`epoll` mode**: A high-performance mode that uses the `epoll` API for scalable I/O.
    * **`thread` mode**: A baseline "thread-per-connection" mode for comparison.
* **TCP and UDP Multicast**: The server forwards messages to all connected TCP clients and broadcasts them via UDP multicast for applications that require the lowest possible latency.
* **Lock-Free Queue**: A lock-free queue is used for efficient message passing between the I/O thread and the broadcaster thread, minimizing contention and improving performance.
* **Python Client Bots**: A Python-based client bot is provided to simulate a large number of clients for load testing.

---

## Getting Started

### Prerequisites

* A C++17 compliant compiler (e.g., g++)
* CMake (version 3.10 or higher)
* Python 3

### Building the Server and Client

1.  **Create a build directory:**
    ```sh
    mkdir build && cd build
    ```
2.  **Generate the Makefiles:**
    ```sh
    cmake ..
    ```
3.  **Compile the server and client:**
    ```sh
    make
    ```

---

## Server & Client Options

This project provides different modes for both the server and the clients, allowing you to experiment with various networking models.

### Server Modes: `epoll` vs. `thread`

You can start the C++ server in one of two modes:

* **`epoll`**: This is the high-performance option. It uses the `epoll` system call on Linux to handle many client connections simultaneously with a single thread. This is highly efficient and scalable, as it avoids the overhead of creating a new thread for every client. The server waits for events on multiple sockets at once, processing data only when it's available.
* **`thread`**: This is a more traditional "thread-per-connection" model. For every client that connects, the server spawns a dedicated new thread to handle all communication with that client. This approach is simpler to understand but becomes inefficient and consumes a lot of resources as the number of clients grows.

### Client Modes: `tcp` vs. `multicast` (UDP)

The Python client bots can operate in two modes:

* **`tcp`**: In this mode, the client establishes a reliable, connection-oriented TCP link to the server. It can both **send and receive** data. All messages sent by TCP clients are forwarded by the server to all other connected TCP clients.
* **`multicast`**: This mode is for **listening only**. The client does not connect directly to the server. Instead, it subscribes to a specific UDP multicast group. The server broadcasts every message it processes to this multicast group. This is extremely fast for one-to-many data distribution as the server sends each message only once to the network, which then delivers it to all subscribed listeners.

---

## Running the Clients

### C++ TCP Client (Send & Receive)

The compiled C++ client provides an interactive shell to send and receive messages from the server.

```sh
./build/low_latency_client
```

---

### Python UDP Multicast Client (Listen-Only)

The Python script can be used to listen to the UDP multicast broadcast from the server.

```sh
python3 python_clients/bot.py --mode multicast
```

---

### License

This project is licensed under the MIT License - see the LICENSE file for details.

---

## Resources

| Topic | Link |
| :--- | :--- |
| **C++ Basics** | [https://www.learncpp.com/](https://www.learncpp.com/) |
| **Network Programming** | [https://beej.us/guide/bgnet/html/index-wide.html](https://beej.us/guide/bgnet/html/index-wide.html) |
| **epoll** | [https://man7.org/linux/man-pages/man7/epoll.7.html](https://man7.org/linux/man-pages/man7/epoll.7.html) |