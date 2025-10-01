#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

// --- Configuration ---
constexpr int TCP_PORT = 8080;
constexpr int MAX_CLIENTS = 1024;
constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_EVENTS = 128;
const char* MCAST_GROUP = "239.0.0.1";
constexpr int MCAST_PORT = 8081;

template<typename T, size_t Size>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {}

    bool push(const T& value) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (tail + 1) % Size;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        buffer_[tail] = value;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& value) {
        size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        value = buffer_[head];
        head_.store((head + 1) % Size, std::memory_order_release);
        return true;
    }

private:
    T buffer_[Size];
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

struct Message {
    int sender_fd;
    char data[BUFFER_SIZE];
    ssize_t length;
};

LockFreeQueue<Message, 2048> message_queue;
std::unordered_map<int, sockaddr_in> tcp_clients; 
std::mutex clients_mutex; 

void set_non_blocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
}

void broadcaster_thread_func() {
    int mcast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mcast_sock < 0) {
        perror("multicast socket");
        return;
    }

    int ttl = 4;
    setsockopt(mcast_sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    struct sockaddr_in mcast_addr;
    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_addr.s_addr = inet_addr(MCAST_GROUP);
    mcast_addr.sin_port = htons(MCAST_PORT);

    std::cout << "[Broadcaster] Ready to send messages via TCP and UDP Multicast." << std::endl;

    Message msg;
    while (true) {
        if (message_queue.pop(msg)) {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (const auto& pair : tcp_clients) {
                int client_fd = pair.first;
                if (client_fd != msg.sender_fd) {
                    send(client_fd, msg.data, msg.length, 0);
                }
            }

            sendto(mcast_sock, msg.data, msg.length, 0, (struct sockaddr*)&mcast_addr, sizeof(mcast_addr));
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    close(mcast_sock);
}

void run_epoll_server() {
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event event, events[MAX_EVENTS];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    set_non_blocking(server_fd);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }

    std::cout << "[Epoll Server] Listening on port " << TCP_PORT << std::endl;

    std::thread broadcaster(broadcaster_thread_func);
    broadcaster.detach();

    char buffer[BUFFER_SIZE];

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

                set_non_blocking(client_fd);
                event.events = EPOLLIN | EPOLLET; // Edge-triggered
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                    continue;
                }
                
                std::lock_guard<std::mutex> lock(clients_mutex);
                tcp_clients[client_fd] = client_addr;
                std::cout << "New connection: " << inet_ntoa(client_addr.sin_addr) << std::endl;

            } else {
                int client_fd = events[i].data.fd;
                ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
                if (bytes_read <= 0) {
                    std::cout << "Client disconnected: " << client_fd << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    tcp_clients.erase(client_fd);
                } else {
                    Message msg;
                    msg.sender_fd = client_fd;
                    msg.length = bytes_read;
                    memcpy(msg.data, buffer, bytes_read);
                    while(!message_queue.push(msg)) {
                        // Queue is full !!
                    }
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
}


void handle_client_thread(int client_fd) {
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            std::cout << "Client disconnected: " << client_fd << std::endl;
            break;
        }

        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& pair : tcp_clients) {
            if (pair.first != client_fd) {
                send(pair.first, buffer, bytes_read, 0);
            }
        }
    }

    close(client_fd);
    std::lock_guard<std::mutex> lock(clients_mutex);
    tcp_clients.erase(client_fd);
}


void run_thread_per_connection_server() {
    int server_fd;
    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    std::cout << "[Thread-per-Connection Server] Listening on port " << TCP_PORT << std::endl;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::cout << "New connection: " << inet_ntoa(client_addr.sin_addr) << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (tcp_clients.size() >= MAX_CLIENTS) {
                std::cerr << "Max clients reached. Rejecting connection." << std::endl;
                close(client_fd);
                continue;
            }
            tcp_clients[client_fd] = client_addr;
        }

        std::thread(handle_client_thread, client_fd).detach();
    }
    close(server_fd);
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <epoll|thread>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "epoll") {
        run_epoll_server();
    } else if (mode == "thread") {
        run_thread_per_connection_server();
    } else {
        std::cerr << "Invalid mode. Use 'epoll' or 'thread'." << std::endl;
        return 1;
    }

    return 0;
}