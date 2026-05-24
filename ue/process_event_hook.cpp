#include "ue/process_event_hook.hpp"

#include "SDK/CoreUObject_classes.hpp"
#include "util/log.hpp"
#include "util/memory.hpp"

#include <MinHook.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

namespace votv::ue::process_event_hook {
namespace {

using ProcessEventFn = void(__fastcall*)(SDK::UObject* object, SDK::UFunction* function, void* params);

ProcessEventFn g_original = nullptr;
std::atomic_bool g_installed = false;
std::atomic_bool g_logging = false;
std::mutex g_queue_mutex;
std::deque<std::function<void()>> g_game_thread_queue;
thread_local bool g_running_queue = false;

void drain_game_thread_queue()
{
    if (g_running_queue) {
        return;
    }

    g_running_queue = true;
    for (int i = 0; i < 4; ++i) {
        std::function<void()> callback;
        {
            std::scoped_lock lock(g_queue_mutex);
            if (g_game_thread_queue.empty()) {
                break;
            }
            callback = std::move(g_game_thread_queue.front());
            g_game_thread_queue.pop_front();
        }

        if (callback) {
            callback();
        }
    }
    g_running_queue = false;
}

void __fastcall hook(SDK::UObject* object, SDK::UFunction* function, void* params)
{
    drain_game_thread_queue();

    if (g_logging && object && function) {
        log::write_format("ProcessEvent: %s -> %s", object->GetName().c_str(), function->GetName().c_str());
    }

    g_original(object, function, params);
}

} // namespace

bool install(bool enable_logging)
{
    g_logging = enable_logging;

    if (g_installed) {
        return true;
    }

    auto* target = reinterpret_cast<void*>(memory::absolute(SDK::Offsets::ProcessEvent));
    if (MH_CreateHook(target, &hook, reinterpret_cast<void**>(&g_original)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        log::write("ProcessEvent hook install failed");
        return false;
    }

    g_installed = true;
    log::write("ProcessEvent hook installed");
    return true;
}

void uninstall()
{
    if (!g_installed) {
        return;
    }

    MH_DisableHook(reinterpret_cast<void*>(memory::absolute(SDK::Offsets::ProcessEvent)));
    g_original = nullptr;
    g_installed = false;
    g_logging = false;
    {
        std::scoped_lock lock(g_queue_mutex);
        g_game_thread_queue.clear();
    }
    log::write("ProcessEvent hook removed");
}

bool installed()
{
    return g_installed;
}

void set_logging(bool enabled)
{
    if (enabled && !g_installed) {
        install(true);
        return;
    }

    g_logging = enabled;
}

bool logging_enabled()
{
    return g_logging;
}

void enqueue_game_thread(std::function<void()> callback)
{
    if (!callback) {
        return;
    }

    if (!g_installed) {
        install(false);
    }

    std::scoped_lock lock(g_queue_mutex);
    g_game_thread_queue.push_back(std::move(callback));
}

} // namespace votv::ue::process_event_hook
