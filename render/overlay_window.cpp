#include "render/overlay_window.hpp"

#include "dll/trainer.hpp"
#include "util/log.hpp"
#include "util/win32.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <chrono>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace votv::render::overlay_window {
namespace {

std::atomic_bool g_running = false;
std::atomic_bool g_stop = false;
HANDLE g_thread = nullptr;
HWND g_window = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_render_target = nullptr;

void cleanup_render_target()
{
    if (g_render_target) {
        g_render_target->Release();
        g_render_target = nullptr;
    }
}

bool create_render_target()
{
    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
        return false;
    }

    const HRESULT hr = g_device->CreateRenderTargetView(back_buffer, nullptr, &g_render_target);
    back_buffer->Release();
    return SUCCEEDED(hr);
}

void cleanup_device()
{
    cleanup_render_target();
    if (g_swap_chain) {
        g_swap_chain->Release();
        g_swap_chain = nullptr;
    }
    if (g_context) {
        g_context->Release();
        g_context = nullptr;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
}

bool create_device(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL selected{};
    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
        &desc, &g_swap_chain, &g_device, &selected, &g_context);

    if (FAILED(hr)) {
        log::write_format("Overlay D3D11 device creation failed: 0x%08X", static_cast<unsigned int>(hr));
        return false;
    }

    return create_render_target();
}

void update_overlay_bounds()
{
    const HWND game = win32::find_game_window();
    if (!game || !g_window) {
        return;
    }

    RECT rect{};
    if (!GetWindowRect(game, &rect)) {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetWindowPos(g_window, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void update_clickthrough()
{
    if (!g_window) {
        return;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(g_window, GWL_EXSTYLE);
    if (votv::trainer::instance().menu_open()) {
        ex_style &= ~WS_EX_TRANSPARENT;
    } else {
        ex_style |= WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(g_window, GWL_EXSTYLE, ex_style);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (votv::trainer::instance().menu_open() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return TRUE;
    }

    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SIZE:
        if (g_device && wparam != SIZE_MINIMIZED) {
            cleanup_render_target();
            g_swap_chain->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0);
            create_render_target();
        }
        return 0;
    case WM_DESTROY:
        g_stop = true;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

DWORD WINAPI overlay_thread(void*)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VotVTrainerOverlay";
    RegisterClassExW(&wc);

    g_window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"VotV Trainer Overlay",
        WS_POPUP,
        0,
        0,
        1280,
        720,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if (!g_window) {
        log::write_format("Overlay window creation failed: %lu", GetLastError());
        g_running = false;
        return 1;
    }

    SetLayeredWindowAttributes(g_window, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margins{ -1 };
    DwmExtendFrameIntoClientArea(g_window, &margins);

    if (!create_device(g_window)) {
        DestroyWindow(g_window);
        g_window = nullptr;
        g_running = false;
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(9.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.065f, 0.085f, 0.96f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.085f, 0.110f, 0.90f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.120f, 0.140f, 0.180f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.170f, 0.210f, 0.270f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.200f, 0.270f, 0.350f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.040f, 0.050f, 0.070f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.070f, 0.100f, 0.140f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.360f, 0.780f, 1.000f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.310f, 0.670f, 0.950f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.420f, 0.820f, 1.000f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.120f, 0.220f, 0.310f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.170f, 0.330f, 0.470f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.220f, 0.430f, 0.600f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.120f, 0.220f, 0.310f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.170f, 0.330f, 0.470f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.220f, 0.430f, 0.600f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.080f, 0.120f, 0.170f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.180f, 0.360f, 0.520f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.130f, 0.260f, 0.380f, 1.00f);
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);

    ShowWindow(g_window, SW_SHOW);
    log::write("Standalone overlay window started");

    MSG msg{};
    while (!g_stop && !win32::unload_requested()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        update_overlay_bounds();
        update_clickthrough();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        votv::trainer::instance().render_menu();
        ImGui::Render();

        constexpr float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_context->OMSetRenderTargets(1, &g_render_target, nullptr);
        g_context->ClearRenderTargetView(g_render_target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device();
    DestroyWindow(g_window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    g_window = nullptr;
    g_running = false;
    log::write("Standalone overlay window stopped");
    return 0;
}

} // namespace

bool start()
{
    if (g_running) {
        return true;
    }

    g_stop = false;
    g_running = true;
    g_thread = CreateThread(nullptr, 0, overlay_thread, nullptr, 0, nullptr);
    if (!g_thread) {
        g_running = false;
        log::write_format("Overlay thread creation failed: %lu", GetLastError());
        return false;
    }

    CloseHandle(g_thread);
    g_thread = nullptr;
    return true;
}

void stop()
{
    g_stop = true;
}

bool running()
{
    return g_running;
}

} // namespace votv::render::overlay_window
