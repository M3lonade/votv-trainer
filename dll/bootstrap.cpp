#include "dll/bootstrap.hpp"

#include "dll/trainer.hpp"
#include "render/dx12_hook.hpp"
#include "render/imgui_layer.hpp"
#include "render/overlay_window.hpp"
#include "render/wndproc_hook.hpp"
#include "ue/process_event_hook.hpp"
#include "util/log.hpp"
#include "util/win32.hpp"

#include <MinHook.h>

#include <chrono>
#include <thread>

namespace votv {

DWORD WINAPI bootstrap_thread(void* module)
{
    log::init();
    log::write("Bootstrap thread started");

    if (MH_Initialize() != MH_OK) {
        log::write("MinHook initialization failed");
        log::shutdown();
        FreeLibraryAndExitThread(static_cast<HMODULE>(module), 1);
    }

    trainer::instance().initialize();

    log::write("Waiting for game window...");
    while (!win32::find_game_window()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    log::write_format("Game window found: 0x%p", win32::find_game_window());
    win32::log_loaded_renderer_modules();

    log::write("Installing in-game DX12 renderer hook");
    if (!render::dx12_hook::install()) {
        log::write("DX12 hook install failed; falling back to standalone overlay renderer");
        if (!render::overlay_window::start()) {
            log::write("Standalone overlay start failed");
        }
    }

    log::write("Trainer is loaded. Press Insert to toggle the menu, End to unload.");

    while (!win32::unload_requested()) {
        if (win32::is_key_pressed(VK_INSERT)) {
            trainer::instance().toggle_menu();
        }
        if (win32::is_key_pressed(VK_END)) {
            win32::request_unload();
        }

        trainer::instance().tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    log::write("Unloading trainer");
    render::overlay_window::stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ue::process_event_hook::uninstall();
    render::wndproc_hook::uninstall();
    render::dx12_hook::uninstall();
    render::imgui_layer::shutdown();
    trainer::instance().shutdown();
    MH_Uninitialize();
    log::shutdown();

    FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
}

} // namespace votv
