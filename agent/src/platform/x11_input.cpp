#include "platform/x11_input.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <cstdlib>
#include <mutex>

namespace {
Display* g_display = nullptr;
std::mutex g_mutex;

unsigned int button_code(const std::string& button) {
    if (button == "left") return Button1;
    if (button == "middle") return Button2;
    if (button == "right") return Button3;
    return 0;
}
}

namespace remote::x11_input {

bool initialize(std::string& error) {
    std::lock_guard lock(g_mutex);

    if (g_display != nullptr) return true;

    g_display = XOpenDisplay(nullptr);
    if (!g_display) {
        error = "Unable to open X11 display. The current session may be Wayland.";
        return false;
    }

    int event_base = 0;
    int error_base = 0;
    int major = 0;
    int minor = 0;
    if (!XTestQueryExtension(g_display, &event_base, &error_base, &major, &minor)) {
        XCloseDisplay(g_display);
        g_display = nullptr;
        error = "XTest extension is not available.";
        return false;
    }

    XSync(g_display, False);
    return true;
}

void shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_display) {
        XCloseDisplay(g_display);
        g_display = nullptr;
    }
}

bool move_mouse(int dx, int dy, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (!g_display) {
        error = "X11 input backend is not initialized.";
        return false;
    }

    if (!XTestFakeRelativeMotionEvent(g_display, dx, dy, CurrentTime)) {
        error = "XTest failed to move the mouse.";
        return false;
    }
    XFlush(g_display);
    return true;
}

bool mouse_button(const std::string& button, bool down, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (!g_display) {
        error = "X11 input backend is not initialized.";
        return false;
    }

    const auto code = button_code(button);
    if (code == 0) {
        error = "Unsupported mouse button: " + button;
        return false;
    }

    XTestFakeButtonEvent(g_display, code, down ? True : False, CurrentTime);
    XFlush(g_display);
    return true;
}

bool scroll(int dx, int dy, std::string& error) {
    std::lock_guard lock(g_mutex);
    if (!g_display) {
        error = "X11 input backend is not initialized.";
        return false;
    }

    const auto emit = [&](int positive_button, int count) {
        const int direction = count > 0 ? 1 : -1;
        for (int i = 0; i < std::abs(count); ++i) {
            const int actual = direction > 0 ? positive_button : positive_button + 1;
            XTestFakeButtonEvent(g_display, actual, True, CurrentTime);
            XTestFakeButtonEvent(g_display, actual, False, CurrentTime);
        }
    };

    // X11 wheel buttons: 4/5 vertical and 6/7 horizontal.
    emit(6, dx);
    emit(4, dy);
    XFlush(g_display);
    return true;
}

}
