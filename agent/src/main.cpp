#include "network/discovery.hpp"
#include "network/server.hpp"
#include "platform/input.hpp"

#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t g_stop = 0;

void signal_handler(int) {
    g_stop = 1;
}
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    constexpr std::uint16_t port = 48721;

    std::cout << "Remote Control Agent v0.1.0\n";

    std::string error;
    if (!remote::input::initialize(error)) {
        std::cerr << "Input backend initialization failed: " << error << '\n';
        return 1;
    }

    remote::discovery::Service discovery("Remote Control PC", port);
    if (!discovery.start(error)) {
        std::cerr << "LAN discovery initialization failed: " << error << '\n';
        std::cerr << "The TCP server will still start.\n";
    } else {
        std::cout << "LAN discovery service initialized.\n";
    }

    remote::network::Server server(port);
    if (!server.run(error)) {
        std::cerr << "Server failed: " << error << '\n';
        discovery.stop();
        remote::input::shutdown();
        return 1;
    }

    discovery.stop();
    remote::input::shutdown();
    return 0;
}
