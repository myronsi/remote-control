#include "network/server.hpp"
#include "platform/input.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <string_view>

namespace {

constexpr std::size_t kMaxLineSize = 16 * 1024;

std::string json_string_value(std::string_view line, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto key_pos = line.find(needle);
    if (key_pos == std::string_view::npos) return {};

    const auto colon = line.find(':', key_pos + needle.size());
    if (colon == std::string_view::npos) return {};
    const auto first_quote = line.find('"', colon + 1);
    if (first_quote == std::string_view::npos) return {};
    const auto second_quote = line.find('"', first_quote + 1);
    if (second_quote == std::string_view::npos) return {};

    return std::string(line.substr(first_quote + 1, second_quote - first_quote - 1));
}

int json_int_value(std::string_view line, std::string_view key, int fallback = 0) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto key_pos = line.find(needle);
    if (key_pos == std::string_view::npos) return fallback;

    const auto colon = line.find(':', key_pos + needle.size());
    if (colon == std::string_view::npos) return fallback;

    std::size_t pos = colon + 1;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

    try {
        return std::stoi(std::string(line.substr(pos)));
    } catch (...) {
        return fallback;
    }
}

bool has_type(std::string_view line, std::string_view type) {
    return json_string_value(line, "type") == type;
}

std::string make_error(const std::string& message) {
    return "{\"version\":1,\"type\":\"error\",\"message\":\"" + message + "\"}\n";
}

bool send_line(int fd, const std::string& line) {
    std::size_t sent = 0;
    while (sent < line.size()) {
        const auto result = ::send(fd, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
        if (result <= 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

std::string handle_command(std::string_view line) {
    if (has_type(line, "hello")) {
        return "{\"version\":1,\"type\":\"hello_ack\",\"agent\":\"remote-control-agent\",\"protocol\":1}\n";
    }

    if (has_type(line, "ping")) {
        return "{\"version\":1,\"type\":\"pong\"}\n";
    }

    std::string error;

    if (has_type(line, "mouse_move")) {
        const int dx = json_int_value(line, "dx");
        const int dy = json_int_value(line, "dy");
        if (!remote::input::move_mouse(dx, dy, error)) return make_error(error);
        return "{\"version\":1,\"type\":\"ok\"}\n";
    }

    if (has_type(line, "mouse_button")) {
        const auto button = json_string_value(line, "button");
        const auto action = json_string_value(line, "action");
        if (action != "down" && action != "up") {
            return make_error("Invalid mouse button action");
        }
        if (!remote::input::mouse_button(button, action == "down", error)) return make_error(error);
        return "{\"version\":1,\"type\":\"ok\"}\n";
    }

    if (has_type(line, "mouse_scroll")) {
        const int dx = json_int_value(line, "dx");
        const int dy = json_int_value(line, "dy");
        if (!remote::input::scroll(dx, dy, error)) return make_error(error);
        return "{\"version\":1,\"type\":\"ok\"}\n";
    }

    if (has_type(line, "key")) {
        const auto key = json_string_value(line, "key");
        const auto action = json_string_value(line, "action");
        if (action != "down" && action != "up") {
            return make_error("Invalid keyboard key action");
        }
        if (key.empty()) return make_error("Keyboard key is empty");
        if (!remote::input::key_event(key, action == "down", error)) return make_error(error);
        return "{\"version\":1,\"type\":\"ok\"}\n";
    }

    return make_error("Unknown command");
}

}

namespace remote::network {

Server::Server(std::uint16_t port) : port_(port) {}

Server::~Server() {
    stop();
}

bool Server::run(std::string& error) {
    if (running_) return true;

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        error = std::string("socket: ") + std::strerror(errno);
        return false;
    }

    int reuse = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        error = std::string("bind: ") + std::strerror(errno);
        stop();
        return false;
    }

    if (::listen(listen_fd_, 8) < 0) {
        error = std::string("listen: ") + std::strerror(errno);
        stop();
        return false;
    }

    running_ = true;
    std::cout << "Listening on TCP port " << port_ << "\n";

    while (running_) {
        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);
        const int client_fd = ::accept(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_length
        );

        if (client_fd < 0) {
            if (running_) {
                std::cerr << "accept: " << std::strerror(errno) << '\n';
            }
            continue;
        }

        std::cout << "Client connected from "
                  << inet_ntoa(client_address.sin_addr) << '\n';
        handle_client(client_fd);
        ::close(client_fd);
        std::cout << "Client disconnected\n";
    }

    return true;
}

void Server::stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

void Server::handle_client(int client_fd) {
    std::string buffer;
    buffer.reserve(4096);

    char chunk[4096];
    while (running_) {
        const auto received = ::recv(client_fd, chunk, sizeof(chunk), 0);
        if (received <= 0) return;

        buffer.append(chunk, static_cast<std::size_t>(received));
        if (buffer.size() > kMaxLineSize * 2) {
            send_line(client_fd, make_error("Input buffer exceeded limit"));
            return;
        }

        while (true) {
            const auto newline = buffer.find('\n');
            if (newline == std::string::npos) break;

            std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);

            if (line.size() > kMaxLineSize) {
                if (!send_line(client_fd, make_error("Command too large"))) return;
                continue;
            }

            while (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            if (!send_line(client_fd, handle_command(line))) return;
        }
    }
}

}
