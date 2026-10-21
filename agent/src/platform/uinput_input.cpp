#include "platform/uinput_input.hpp"

#include <linux/input.h>
#include <linux/uinput.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
int g_fd = -1;
std::mutex g_mutex;

bool emit_event(int type, int code, int value, std::string& error) {
    input_event event{};
    event.type = static_cast<__u16>(type);
    event.code = static_cast<__u16>(code);
    event.value = value;

    if (::write(g_fd, &event, sizeof(event)) != static_cast<ssize_t>(sizeof(event))) {
        error = std::string("uinput write failed: ") + std::strerror(errno);
        return false;
    }
    return true;
}

bool sync_events(std::string& error) {
    return emit_event(EV_SYN, SYN_REPORT, 0, error);
}

int button_code(const std::string& button) {
    if (button == "left") return BTN_LEFT;
    if (button == "middle") return BTN_MIDDLE;
    if (button == "right") return BTN_RIGHT;
    return 0;
}

int key_code(const std::string& key) {
    if (key == "ESC") return KEY_ESC;
    if (key == "ENTER") return KEY_ENTER;
    if (key == "TAB") return KEY_TAB;
    if (key == "BACKSPACE") return KEY_BACKSPACE;
    if (key == "SPACE") return KEY_SPACE;
    if (key == "SHIFT") return KEY_LEFTSHIFT;
    if (key == "CTRL") return KEY_LEFTCTRL;
    if (key == "ALT") return KEY_LEFTALT;
    if (key == "META") return KEY_LEFTMETA;
    if (key == "CAPSLOCK") return KEY_CAPSLOCK;
    if (key == "UP") return KEY_UP;
    if (key == "DOWN") return KEY_DOWN;
    if (key == "LEFT") return KEY_LEFT;
    if (key == "RIGHT") return KEY_RIGHT;
    if (key == "DELETE") return KEY_DELETE;
    if (key == "HOME") return KEY_HOME;
    if (key == "END") return KEY_END;
    if (key == "PAGEUP") return KEY_PAGEUP;
    if (key == "PAGEDOWN") return KEY_PAGEDOWN;
    if (key == "INSERT") return KEY_INSERT;
    if (key == "F1") return KEY_F1;
    if (key == "F2") return KEY_F2;
    if (key == "F3") return KEY_F3;
    if (key == "F4") return KEY_F4;
    if (key == "F5") return KEY_F5;
    if (key == "F6") return KEY_F6;
    if (key == "F7") return KEY_F7;
    if (key == "F8") return KEY_F8;
    if (key == "F9") return KEY_F9;
    if (key == "F10") return KEY_F10;
    if (key == "F11") return KEY_F11;
    if (key == "F12") return KEY_F12;

    if (key.size() == 1) {
        const char c = key[0];
        if (c >= 'A' && c <= 'Z') return KEY_A + (c - 'A');
        if (c >= '0' && c <= '9') {
            if (c == '0') return KEY_0;
            return KEY_1 + (c - '1');
        }
    }

    if (key == "-") return KEY_MINUS;
    if (key == "=") return KEY_EQUAL;
    if (key == "[") return KEY_LEFTBRACE;
    if (key == "]") return KEY_RIGHTBRACE;
    if (key == "\\") return KEY_BACKSLASH;
    if (key == ";") return KEY_SEMICOLON;
    if (key == "'") return KEY_APOSTROPHE;
    if (key == "`") return KEY_GRAVE;
    if (key == ",") return KEY_COMMA;
    if (key == ".") return KEY_DOT;
    if (key == "/") return KEY_SLASH;

    return 0;
}
}

namespace remote::uinput_input {

bool initialize(std::string& error) {
    std::lock_guard lock(g_mutex);

    if (g_fd >= 0) return true;

    g_fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_fd < 0) {
        error = std::string("Unable to open /dev/uinput: ") + std::strerror(errno) +
                ". Add your user to the input group and log in again.";
        return false;
    }

    auto fail = [&](const char* message) {
        error = message;
        ::close(g_fd);
        g_fd = -1;
        return false;
    };

    if (::ioctl(g_fd, UI_SET_EVBIT, EV_REL) < 0) return fail("Unable to enable relative input events.");
    if (::ioctl(g_fd, UI_SET_EVBIT, EV_KEY) < 0) return fail("Unable to enable key input events.");
    if (::ioctl(g_fd, UI_SET_RELBIT, REL_X) < 0) return fail("Unable to enable X mouse movement.");
    if (::ioctl(g_fd, UI_SET_RELBIT, REL_Y) < 0) return fail("Unable to enable Y mouse movement.");
    if (::ioctl(g_fd, UI_SET_RELBIT, REL_WHEEL) < 0) return fail("Unable to enable vertical scrolling.");
    if (::ioctl(g_fd, UI_SET_RELBIT, REL_HWHEEL) < 0) return fail("Unable to enable horizontal scrolling.");
    if (::ioctl(g_fd, UI_SET_KEYBIT, BTN_LEFT) < 0) return fail("Unable to enable left mouse button.");
    if (::ioctl(g_fd, UI_SET_KEYBIT, BTN_MIDDLE) < 0) return fail("Unable to enable middle mouse button.");
    if (::ioctl(g_fd, UI_SET_KEYBIT, BTN_RIGHT) < 0) return fail("Unable to enable right mouse button.");

    // Expose the keyboard key range on the same virtual input device.
    for (int code = KEY_ESC; code <= KEY_MAX; ++code) {
        if (::ioctl(g_fd, UI_SET_KEYBIT, code) < 0) {
            return fail("Unable to enable keyboard input events.");
        }
    }

    uinput_user_dev device{};
    std::snprintf(device.name, UINPUT_MAX_NAME_SIZE, "Remote Control Virtual Input");
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x1209;
    device.id.product = 0x5243;
    device.id.version = 1;

    if (::write(g_fd, &device, sizeof(device)) != static_cast<ssize_t>(sizeof(device))) {
        return fail("Unable to configure the virtual input device.");
    }

    if (::ioctl(g_fd, UI_DEV_CREATE) < 0) {
        return fail("Unable to create the virtual input device.");
    }

    // Give udev/KWin a short moment to register the new input device.
    ::usleep(100000);
    return true;
}

void shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_fd < 0) return;

    ::ioctl(g_fd, UI_DEV_DESTROY);
    ::close(g_fd);
    g_fd = -1;
}

bool move_mouse(int dx, int dy, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (g_fd < 0) {
        error = "uinput backend is not initialized.";
        return false;
    }

    while (dx != 0 || dy != 0) {
        const int step_x = dx > 127 ? 127 : dx < -127 ? -127 : dx;
        const int step_y = dy > 127 ? 127 : dy < -127 ? -127 : dy;

        if (step_x != 0 && !emit_event(EV_REL, REL_X, step_x, error)) return false;
        if (step_y != 0 && !emit_event(EV_REL, REL_Y, step_y, error)) return false;
        if (!sync_events(error)) return false;

        dx -= step_x;
        dy -= step_y;
    }

    return true;
}

bool mouse_button(const std::string& button, bool down, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (g_fd < 0) {
        error = "uinput backend is not initialized.";
        return false;
    }

    const int code = button_code(button);
    if (code == 0) {
        error = "Unsupported mouse button: " + button;
        return false;
    }

    if (!emit_event(EV_KEY, code, down ? 1 : 0, error)) return false;
    return sync_events(error);
}

bool scroll(int dx, int dy, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (g_fd < 0) {
        error = "uinput backend is not initialized.";
        return false;
    }

    if (dy != 0 && !emit_event(EV_REL, REL_WHEEL, dy, error)) return false;
    if (dx != 0 && !emit_event(EV_REL, REL_HWHEEL, dx, error)) return false;
    return sync_events(error);
}

bool key_event(const std::string& key, bool down, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (g_fd < 0) {
        error = "uinput backend is not initialized.";
        return false;
    }

    const int code = key_code(key);
    if (code == 0) {
        error = "Unsupported keyboard key: " + key;
        return false;
    }

    if (!emit_event(EV_KEY, code, down ? 1 : 0, error)) return false;
    return sync_events(error);
}

}
