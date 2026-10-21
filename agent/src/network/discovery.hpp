#pragma once

#include <cstdint>
#include <string>

namespace remote::discovery {

class Service {
public:
    Service(std::string instance_name, std::uint16_t port);
    ~Service();

    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;

    bool start(std::string& error);
    void stop();

private:
    std::string instance_name_;
    std::uint16_t port_;
    bool running_ = false;
    int fd_ = -1;
};

}
