#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

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
    std::thread thread_;
    std::shared_ptr<std::atomic_bool> stop_requested_;
};

}
