cmake_minimum_required(VERSION 3.16)

project(RemoteControlAgent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(AVAHI REQUIRED IMPORTED_TARGET avahi-client)

add_executable(remote-control-agent
    src/main.cpp
    src/network/server.cpp
    src/network/discovery.cpp
    src/platform/input.cpp
    src/platform/uinput_input.cpp
)

target_include_directories(remote-control-agent PRIVATE src)

target_link_libraries(remote-control-agent PRIVATE
    Threads::Threads
    PkgConfig::AVAHI
)
