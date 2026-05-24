#include "render/imgui_layer.hpp"

#include "dll/trainer.hpp"
#include "render/theme.hpp"
#include "render/wndproc_hook.hpp"
#include "util/log.hpp"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace votv::render::imgui_layer {
namespace {

bool g_initialized = false;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;

} // namespace

bool initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (g_initialized) {
        return true;
    }

    if (!hwnd || !device || !context) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    theme::apply_space_theme();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    if (!ImGui_ImplWin32_Init(hwnd) || !ImGui_ImplDX11_Init(device, context)) {
        log::write("ImGui backend initialization failed");
        ImGui::DestroyContext();
        return false;
    }

    wndproc_hook::install(hwnd);
    g_device = device;
    g_context = context;
    g_initialized = true;
    log::write("ImGui initialized");
    return true;
}

void begin_frame()
{
    if (!g_initialized) {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void render()
{
    if (!g_initialized) {
        return;
    }

    trainer::instance().render_menu();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void shutdown()
{
    if (!g_initialized) {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_context = nullptr;
    g_device = nullptr;
    g_initialized = false;
    log::write("ImGui shutdown");
}

bool initialized()
{
    return g_initialized;
}

} // namespace votv::render::imgui_layer
