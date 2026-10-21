#include "network/discovery.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <string>

namespace remote::discovery {

namespace {
constexpr std::uint16_t kMdnsPort = 5353;
constexpr const char* kMdnsAddress = "224.0.0.251";

std::string escape_dns_name(const std::string& name) {
    return name;
}

void append_u16(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>(value & 0xff));
}

void append_dns_name(std::string& out, const std::string& name) {
    std::size_t start = 0;
    while (start < name.size()) {
        const auto dot = name.find('.', start);
        const auto end = dot == std::string::npos ? name.size() : dot;
        const auto length = end - start;
        out.push_back(static_cast<char>(length));
        out.append(name, start, length);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    out.push_back('\0');
}

}

Service::Service(std::string instance_name, std::uint16_t port)
    : instance_name_(std::move(instance_name)), port_(port) {}

Service::~Service() {
    stop();
}

bool Service::start(std::string& error) {
    if (running_) return true;

    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        error = std::string("mDNS socket: ") + std::strerror(errno);
        return false;
    }

    int reuse = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(kMdnsPort);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        error = std::string("mDNS bind: ") + std::strerror(errno);
        stop();
        return false;
    }

    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = inet_addr(kMdnsAddress);
    membership.imr_interface.s_addr = htonl(INADDR_ANY);

    if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) < 0) {
        error = std::string("mDNS membership: ") + std::strerror(errno);
        stop();
        return false;
    }

    // The socket is prepared for the mDNS responder. Actual DNS-SD records
    // will be added in the next discovery implementation step.
    running_ = true;
    return true;
}

void Service::stop() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    running_ = false;
}

}
