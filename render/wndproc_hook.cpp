#include "render/wndproc_hook.hpp"

#include "dll/trainer.hpp"
#include "util/log.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace votv::render::wndproc_hook {
namespace {

HWND g_window = nullptr;
WNDPROC g_original = nullptr;

LRESULT CALLBACK hook_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (trainer::instance().menu_open() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return TRUE;
    }

    return CallWindowProcW(g_original, hwnd, msg, wparam, lparam);
}

} // namespace

bool install(HWND hwnd)
{
    if (g_original || !hwnd) {
        return g_original != nullptr;
    }

    g_window = hwnd;
    g_original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hook_proc)));
    if (!g_original) {
        log::write_format("SetWindowLongPtrW failed: %lu", GetLastError());
        g_window = nullptr;
        return false;
    }

    log::write("WndProc hook installed");
    return true;
}

void uninstall()
{
    if (g_window && g_original) {
        SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original));
        log::write("WndProc hook removed");
    }

    g_window = nullptr;
    g_original = nullptr;
}

HWND window()
{
    return g_window;
}

} // namespace votv::render::wndproc_hook
