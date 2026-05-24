#include "render/dx12_hook.hpp"

#include "dll/trainer.hpp"
#include "render/theme.hpp"
#include "render/wndproc_hook.hpp"
#include "util/log.hpp"
#include "util/win32.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <MinHook.h>

#include <atomic>
#include <vector>

namespace votv::render::dx12_hook {
namespace {

struct FrameContext {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* render_target = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
};

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags);
using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue* queue, UINT command_lists_count, ID3D12CommandList* const* command_lists);

PresentFn g_original_present = nullptr;
ResizeBuffersFn g_original_resize_buffers = nullptr;
ExecuteCommandListsFn g_original_execute_command_lists = nullptr;
void* g_present_address = nullptr;
void* g_resize_buffers_address = nullptr;
void* g_execute_command_lists_address = nullptr;

std::atomic_bool g_installed = false;
std::atomic_bool g_initialized = false;
std::atomic_bool g_seen_present = false;

ID3D12Device* g_device = nullptr;
ID3D12CommandQueue* g_command_queue = nullptr;
ID3D12GraphicsCommandList* g_command_list = nullptr;
ID3D12DescriptorHeap* g_rtv_heap = nullptr;
ID3D12DescriptorHeap* g_srv_heap = nullptr;
DXGI_FORMAT g_rtv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
UINT g_rtv_descriptor_count = 0;
std::vector<FrameContext> g_frames;

void wait_for_gpu()
{
    if (!g_device || !g_command_queue) {
        return;
    }

    ID3D12Fence* fence = nullptr;
    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
        return;
    }

    HANDLE event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_handle) {
        fence->Release();
        return;
    }

    constexpr UINT64 fence_value = 1;
    if (SUCCEEDED(g_command_queue->Signal(fence, fence_value)) && fence->GetCompletedValue() < fence_value) {
        if (SUCCEEDED(fence->SetEventOnCompletion(fence_value, event_handle))) {
            WaitForSingleObject(event_handle, 2000);
        }
    }

    CloseHandle(event_handle);
    fence->Release();
}

void release_frames()
{
    for (auto& frame : g_frames) {
        if (frame.render_target) {
            frame.render_target->Release();
            frame.render_target = nullptr;
        }
        if (frame.allocator) {
            frame.allocator->Release();
            frame.allocator = nullptr;
        }
    }
    g_frames.clear();
}

void release_dx12()
{
    release_frames();
    if (g_command_list) {
        g_command_list->Release();
        g_command_list = nullptr;
    }
    if (g_rtv_heap) {
        g_rtv_heap->Release();
        g_rtv_heap = nullptr;
    }
    g_rtv_descriptor_count = 0;
    if (g_srv_heap) {
        g_srv_heap->Release();
        g_srv_heap = nullptr;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
    g_initialized = false;
}

void release_swap_chain_resources()
{
    wait_for_gpu();
    release_frames();
    if (g_rtv_heap) {
        g_rtv_heap->Release();
        g_rtv_heap = nullptr;
    }
    g_rtv_descriptor_count = 0;
}

bool create_render_targets(IDXGISwapChain3* swap_chain, UINT buffer_count)
{
    if (!g_rtv_heap || g_rtv_descriptor_count < buffer_count) {
        if (g_rtv_heap) {
            g_rtv_heap->Release();
            g_rtv_heap = nullptr;
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
        rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_desc.NumDescriptors = buffer_count;
        rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(g_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&g_rtv_heap)))) {
            return false;
        }
        g_rtv_descriptor_count = buffer_count;
    }

    const UINT descriptor_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    g_frames.resize(buffer_count);
    for (UINT i = 0; i < buffer_count; ++i) {
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frames[i].allocator)))) {
            return false;
        }

        if (FAILED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&g_frames[i].render_target)))) {
            return false;
        }

        g_frames[i].rtv = rtv;
        g_device->CreateRenderTargetView(g_frames[i].render_target, nullptr, rtv);
        rtv.ptr += descriptor_size;
    }

    return true;
}

bool initialize_imgui(IDXGISwapChain* swap_chain)
{
    if (g_initialized) {
        return true;
    }

    if (!g_command_queue) {
        return false;
    }

    IDXGISwapChain3* swap_chain3 = nullptr;
    if (FAILED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain3)))) {
        log::write("DX12 swap chain QueryInterface IDXGISwapChain3 failed");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    swap_chain->GetDesc(&desc);
    g_rtv_format = desc.BufferDesc.Format;
    const UINT buffer_count = desc.BufferCount;

    if (FAILED(swap_chain->GetDevice(IID_PPV_ARGS(&g_device)))) {
        log::write("DX12 Present could not get ID3D12Device");
        swap_chain3->Release();
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.NumDescriptors = 1;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_device->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&g_srv_heap)))) {
        log::write("DX12 SRV heap creation failed");
        swap_chain3->Release();
        return false;
    }

    if (!create_render_targets(swap_chain3, buffer_count)) {
        log::write("DX12 render target setup failed");
        swap_chain3->Release();
        return false;
    }

    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frames[0].allocator, nullptr, IID_PPV_ARGS(&g_command_list)))) {
        log::write("DX12 command list creation failed");
        swap_chain3->Release();
        return false;
    }
    g_command_list->Close();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    theme::apply_space_theme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(desc.OutputWindow ? desc.OutputWindow : win32::find_game_window());

    ImGui_ImplDX12_InitInfo init_info{};
    init_info.Device = g_device;
    init_info.CommandQueue = g_command_queue;
    init_info.NumFramesInFlight = static_cast<int>(buffer_count);
    init_info.RTVFormat = g_rtv_format;
    init_info.SrvDescriptorHeap = g_srv_heap;
    init_info.LegacySingleSrvCpuDescriptor = g_srv_heap->GetCPUDescriptorHandleForHeapStart();
    init_info.LegacySingleSrvGpuDescriptor = g_srv_heap->GetGPUDescriptorHandleForHeapStart();
    if (!ImGui_ImplDX12_Init(&init_info)) {
        log::write("ImGui DX12 backend initialization failed");
        swap_chain3->Release();
        return false;
    }

    wndproc_hook::install(desc.OutputWindow ? desc.OutputWindow : win32::find_game_window());
    g_initialized = true;
    log::write("DX12 ImGui initialized in-game");
    swap_chain3->Release();
    return true;
}

void render_imgui(IDXGISwapChain* swap_chain)
{
    if (!initialize_imgui(swap_chain) || !g_initialized) {
        return;
    }

    IDXGISwapChain3* swap_chain3 = nullptr;
    if (FAILED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain3)))) {
        return;
    }

    const UINT frame_index = swap_chain3->GetCurrentBackBufferIndex();
    if (g_frames.empty()) {
        DXGI_SWAP_CHAIN_DESC desc{};
        swap_chain->GetDesc(&desc);
        g_rtv_format = desc.BufferDesc.Format;
        if (!create_render_targets(swap_chain3, desc.BufferCount)) {
            log::write("DX12 render target recreation failed after resize");
            swap_chain3->Release();
            return;
        }
        log::write("DX12 render targets recreated after resize");
    }
    swap_chain3->Release();
    if (frame_index >= g_frames.size()) {
        return;
    }

    FrameContext& frame = g_frames[frame_index];
    frame.allocator->Reset();
    g_command_list->Reset(frame.allocator, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.render_target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_command_list->ResourceBarrier(1, &barrier);

    g_command_list->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { g_srv_heap };
    g_command_list->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    trainer::instance().render_menu();
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_command_list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_command_list->ResourceBarrier(1, &barrier);
    g_command_list->Close();

    ID3D12CommandList* command_lists[] = { g_command_list };
    g_command_queue->ExecuteCommandLists(1, command_lists);
}

HRESULT __stdcall present_hook(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    if (!g_seen_present.exchange(true)) {
        log::write("DX12 Present hook is receiving frames");
    }

    render_imgui(swap_chain);
    return g_original_present(swap_chain, sync_interval, flags);
}

HRESULT __stdcall resize_buffers_hook(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags)
{
    log::write_format(
        "DX12 ResizeBuffers: buffers=%u size=%ux%u format=0x%X flags=0x%X",
        buffer_count,
        width,
        height,
        static_cast<unsigned int>(new_format),
        swap_chain_flags);

    release_swap_chain_resources();
    return g_original_resize_buffers(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
}

void __stdcall execute_command_lists_hook(ID3D12CommandQueue* queue, UINT command_lists_count, ID3D12CommandList* const* command_lists)
{
    if (!g_command_queue && queue) {
        g_command_queue = queue;
        log::write_format("Captured ID3D12CommandQueue: 0x%p", queue);
    }

    g_original_execute_command_lists(queue, command_lists_count, command_lists);
}

bool create_dummy_targets(void** present, void** resize_buffers, void** execute_command_lists)
{
    *present = nullptr;
    *resize_buffers = nullptr;
    *execute_command_lists = nullptr;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VotVTrainerD3D12DummyWindow";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGIFactory4* factory = nullptr;
    IDXGISwapChain* swap_chain = nullptr;

    bool ok = false;
    if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (SUCCEEDED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue))) &&
            SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            DXGI_SWAP_CHAIN_DESC desc{};
            desc.BufferCount = 2;
            desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.OutputWindow = hwnd;
            desc.SampleDesc.Count = 1;
            desc.Windowed = TRUE;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

            if (SUCCEEDED(factory->CreateSwapChain(queue, &desc, &swap_chain))) {
                *present = (*reinterpret_cast<void***>(swap_chain))[8];
                *resize_buffers = (*reinterpret_cast<void***>(swap_chain))[13];
                *execute_command_lists = (*reinterpret_cast<void***>(queue))[10];
                ok = *present && *resize_buffers && *execute_command_lists;
            }
        }
    }

    if (swap_chain) {
        swap_chain->Release();
    }
    if (factory) {
        factory->Release();
    }
    if (queue) {
        queue->Release();
    }
    if (device) {
        device->Release();
    }
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}

} // namespace

bool install()
{
    if (g_installed) {
        return true;
    }

    void* present = nullptr;
    void* resize_buffers = nullptr;
    void* execute_command_lists = nullptr;
    if (!create_dummy_targets(&present, &resize_buffers, &execute_command_lists)) {
        log::write("DX12 dummy target creation failed");
        return false;
    }

    log::write_format("DX12 Present address: 0x%p", present);
    log::write_format("DX12 ResizeBuffers address: 0x%p", resize_buffers);
    log::write_format("DX12 ExecuteCommandLists address: 0x%p", execute_command_lists);
    g_present_address = present;
    g_resize_buffers_address = resize_buffers;
    g_execute_command_lists_address = execute_command_lists;

    MH_STATUS status = MH_CreateHook(execute_command_lists, &execute_command_lists_hook, reinterpret_cast<void**>(&g_original_execute_command_lists));
    if (status != MH_OK) {
        log::write_format("Failed to create ExecuteCommandLists hook: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_CreateHook(present, &present_hook, reinterpret_cast<void**>(&g_original_present));
    if (status != MH_OK) {
        log::write_format("Failed to create DX12 Present hook: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_CreateHook(resize_buffers, &resize_buffers_hook, reinterpret_cast<void**>(&g_original_resize_buffers));
    if (status != MH_OK) {
        log::write_format("Failed to create DX12 ResizeBuffers hook: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(execute_command_lists);
    if (status != MH_OK) {
        log::write_format("Failed to enable ExecuteCommandLists hook: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(present);
    if (status != MH_OK) {
        log::write_format("Failed to enable DX12 Present hook: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(resize_buffers);
    if (status != MH_OK) {
        log::write_format("Failed to enable DX12 ResizeBuffers hook: %s", MH_StatusToString(status));
        return false;
    }

    g_installed = true;
    log::write("DX12 hooks installed");
    return true;
}

void uninstall()
{
    if (!g_installed) {
        return;
    }

    if (g_present_address) {
        MH_DisableHook(g_present_address);
    }
    if (g_resize_buffers_address) {
        MH_DisableHook(g_resize_buffers_address);
    }
    if (g_execute_command_lists_address) {
        MH_DisableHook(g_execute_command_lists_address);
    }

    if (g_initialized) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        if (ImGui::GetCurrentContext()) {
            ImGui::DestroyContext();
        }
    }
    release_dx12();
    g_command_queue = nullptr;
    g_present_address = nullptr;
    g_resize_buffers_address = nullptr;
    g_execute_command_lists_address = nullptr;
    g_original_present = nullptr;
    g_original_resize_buffers = nullptr;
    g_original_execute_command_lists = nullptr;
    g_installed = false;
    log::write("DX12 hooks removed");
}

bool initialized()
{
    return g_initialized;
}

} // namespace votv::render::dx12_hook
