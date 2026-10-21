cmake_minimum_required(VERSION 3.16)

project(RemoteControlAgent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(remote-control-agent
    src/main.cpp
    src/network/server.cpp
    src/platform/input.cpp
    src/platform/x11_input.cpp
)

target_include_directories(remote-control-agent PRIVATE src)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(remote-control-agent PRIVATE Threads::Threads)

    find_package(X11 REQUIRED)
    find_library(XTST_LIBRARY Xtst REQUIRED)
    target_include_directories(remote-control-agent PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(remote-control-agent PRIVATE ${X11_LIBRARIES} ${XTST_LIBRARY})
endif()
