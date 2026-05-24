#pragma once

#include <Windows.h>

namespace votv::win32 {

HWND find_game_window();
bool is_key_pressed(int virtual_key);
void log_loaded_renderer_modules();
void request_unload();
bool unload_requested();

} // namespace votv::win32
