#include "util/win32.hpp"

#include "util/log.hpp"

#include <atomic>

namespace votv::win32 {
namespace {

std::atomic_bool g_unload_requested = false;

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lparam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) {
        return TRUE;
    }

    auto* out = reinterpret_cast<HWND*>(lparam);
    *out = hwnd;
    return FALSE;
}

} // namespace

HWND find_game_window()
{
    HWND hwnd = nullptr;
    EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&hwnd));
    return hwnd;
}

bool is_key_pressed(int virtual_key)
{
    return (GetAsyncKeyState(virtual_key) & 1) != 0;
}

void log_loaded_renderer_modules()
{
    struct ModuleCheck {
        const wchar_t* name;
        const char* label;
    };

    constexpr ModuleCheck modules[] = {
        { L"d3d11.dll", "D3D11" },
        { L"d3d12.dll", "D3D12" },
        { L"dxgi.dll", "DXGI" },
        { L"vulkan-1.dll", "Vulkan" },
        { L"opengl32.dll", "OpenGL" },
    };

    log::write("Renderer module check:");
    for (const auto& module : modules) {
        const HMODULE handle = GetModuleHandleW(module.name);
        log::write_format("  %-6s %s 0x%p", module.label, handle ? "loaded at" : "not loaded", handle);
    }
}

void request_unload()
{
    g_unload_requested = true;
}

bool unload_requested()
{
    return g_unload_requested;
}

} // namespace votv::win32
