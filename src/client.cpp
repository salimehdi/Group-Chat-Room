#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int TCP_PORT = 8080;
const char* SERVER_IP = "127.0.0.1";
constexpr int BUFFER_SIZE = 1024;

void receive_messages(int sock) {
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "Server disconnected." << std::endl;
            break;
        }
        buffer[bytes_received] = '\0';
        std::cout << "Received: " << buffer << std::endl;
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TCP_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }

    std::cout << "Connected to server. You can start typing." << std::endl;

    std::thread receiver_thread(receive_messages, sock);
    receiver_thread.detach();

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) {
            send(sock, line.c_str(), line.length(), 0);
        }
    }

    close(sock);
    return 0;
}