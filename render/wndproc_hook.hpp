#pragma once

#include <Windows.h>

namespace votv::render::wndproc_hook {

bool install(HWND hwnd);
void uninstall();
HWND window();

} // namespace votv::render::wndproc_hook
