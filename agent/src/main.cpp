#include "network/server.hpp"
#include "platform/input.hpp"

#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
volatile std::sig_atomic_t g_stop = 0;

void signal_handler(int) {
    g_stop = 1;
}
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "Remote Control Agent v0.1.0\n";

    std::string error;
    if (!remote::input::initialize(error)) {
        std::cerr << "Input backend initialization failed: " << error << '\n';
        return 1;
    }

    remote::network::Server server(48721);
    if (!server.run(error)) {
        std::cerr << "Server failed: " << error << '\n';
        remote::input::shutdown();
        return 1;
    }

    remote::input::shutdown();
    return 0;
}
