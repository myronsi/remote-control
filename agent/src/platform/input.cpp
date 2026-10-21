#include "platform/input.hpp"
#include "platform/x11_input.hpp"

namespace remote::input {

bool initialize(std::string& error) {
    return x11_input::initialize(error);
}

void shutdown() {
    x11_input::shutdown();
}

bool move_mouse(int dx, int dy, std::string& error) {
    return x11_input::move_mouse(dx, dy, error);
}

bool mouse_button(const std::string& button, bool down, std::string& error) {
    return x11_input::mouse_button(button, down, error);
}

bool scroll(int dx, int dy, std::string& error) {
    return x11_input::scroll(dx, dy, error);
}

}
