#include "render/dx11_hook.hpp"

#include "render/imgui_layer.hpp"
#include "util/log.hpp"
#include "util/win32.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>

#include <atomic>
#include <thread>

namespace votv::render::dx11_hook {
namespace {

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

PresentFn g_original_present = nullptr;
bool g_installed = false;
std::atomic_bool g_seen_present = false;
std::atomic_bool g_logged_get_device_failure = false;

HRESULT __stdcall present_hook(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    if (!g_seen_present.exchange(true)) {
        log::write("DX11 Present hook is receiving frames");
    }

    if (!imgui_layer::initialized()) {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;

        if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) && device) {
            device->GetImmediateContext(&context);

            DXGI_SWAP_CHAIN_DESC desc{};
            swap_chain->GetDesc(&desc);

            const HWND hwnd = desc.OutputWindow ? desc.OutputWindow : win32::find_game_window();
            log::write_format("Initializing ImGui on hwnd 0x%p", hwnd);
            if (!imgui_layer::initialize(hwnd, device, context)) {
                log::write("ImGui initialization failed inside Present");
            }

            if (context) {
                context->Release();
            }
            device->Release();
        } else if (!g_logged_get_device_failure.exchange(true)) {
            log::write("Present hook could not get ID3D11Device from swap chain");
            log::write("Falling back to standalone overlay window for rendering");
        }
    }

    imgui_layer::begin_frame();
    imgui_layer::render();

    return g_original_present(swap_chain, sync_interval, flags);
}

void* find_present_address()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VotVTrainerDummyWindow";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;

    void* present = nullptr;
    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &desc, &swap_chain, &device, nullptr, &context);

    if (SUCCEEDED(hr) && swap_chain) {
        auto** vtable = *reinterpret_cast<void***>(swap_chain);
        present = vtable[8];
    } else {
        log::write_format("Dummy D3D11 swap chain creation failed: 0x%08X", static_cast<unsigned int>(hr));
    }

    if (swap_chain) {
        swap_chain->Release();
    }
    if (context) {
        context->Release();
    }
    if (device) {
        device->Release();
    }
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return present;
}

} // namespace

bool install()
{
    if (g_installed) {
        return true;
    }

    void* present = find_present_address();
    if (!present) {
        log::write("Could not locate IDXGISwapChain::Present");
        return false;
    }
    log::write_format("IDXGISwapChain::Present address: 0x%p", present);

    const MH_STATUS create_status = MH_CreateHook(present, &present_hook, reinterpret_cast<void**>(&g_original_present));
    if (create_status != MH_OK) {
        log::write_format("Failed to create Present hook: %s", MH_StatusToString(create_status));
        return false;
    }

    const MH_STATUS enable_status = MH_EnableHook(present);
    if (enable_status != MH_OK) {
        log::write_format("Failed to enable Present hook: %s", MH_StatusToString(enable_status));
        return false;
    }

    g_installed = true;
    log::write("DX11 Present hook installed");
    return true;
}

void uninstall()
{
    if (!g_installed) {
        return;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    g_original_present = nullptr;
    g_installed = false;
    log::write("DX11 Present hook removed");
}

} // namespace votv::render::dx11_hook
