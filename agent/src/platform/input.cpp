#include "platform/input.hpp"
#include "platform/uinput_input.hpp"

namespace remote::input {

bool initialize(std::string& error) {
    return uinput_input::initialize(error);
}

void shutdown() {
    uinput_input::shutdown();
}

bool move_mouse(int dx, int dy, std::string& error) {
    return uinput_input::move_mouse(dx, dy, error);
}

bool mouse_button(const std::string& button, bool down, std::string& error) {
    return uinput_input::mouse_button(button, down, error);
}

bool scroll(int dx, int dy, std::string& error) {
    return uinput_input::scroll(dx, dy, error);
}

}
