#pragma once

#include <string>

namespace remote::input {

bool initialize(std::string& error);
void shutdown();

bool move_mouse(int dx, int dy, std::string& error);
bool mouse_button(const std::string& button, bool down, std::string& error);
bool scroll(int dx, int dy, std::string& error);
bool key_event(const std::string& key, bool down, std::string& error);

}
