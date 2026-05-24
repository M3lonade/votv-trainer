#pragma once

#include <functional>

namespace votv::ue::process_event_hook {

bool install(bool enable_logging);
void uninstall();
bool installed();
void set_logging(bool enabled);
bool logging_enabled();
void enqueue_game_thread(std::function<void()> callback);

} // namespace votv::ue::process_event_hook
