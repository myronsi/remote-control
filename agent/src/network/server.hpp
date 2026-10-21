#pragma once

#include <cstdint>
#include <string>

namespace remote::network {

class Server {
public:
    explicit Server(std::uint16_t port);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool run(std::string& error);
    void stop();

private:
    std::uint16_t port_;
    int listen_fd_ = -1;
    bool running_ = false;

    void handle_client(int client_fd);
};

}
