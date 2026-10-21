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

    uinput_user_dev device{};
    std::snprintf(device.name, UINPUT_MAX_NAME_SIZE, "Remote Control Virtual Mouse");
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x1209;
    device.id.product = 0x5243;
    device.id.version = 1;

    if (::write(g_fd, &device, sizeof(device)) != static_cast<ssize_t>(sizeof(device))) {
        return fail("Unable to configure the virtual mouse device.");
    }

    if (::ioctl(g_fd, UI_DEV_CREATE) < 0) {
        return fail("Unable to create the virtual mouse device.");
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

    // Split very large deltas so a malformed network packet cannot create one
    // unusually large kernel input event.
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

    // The protocol expresses wheel movement in logical ticks. Each tick is one
    // REL_WHEEL/REL_HWHEEL event, matching the Linux input subsystem convention.
    if (dy != 0 && !emit_event(EV_REL, REL_WHEEL, dy, error)) return false;
    if (dx != 0 && !emit_event(EV_REL, REL_HWHEEL, dx, error)) return false;
    return sync_events(error);
}

}
