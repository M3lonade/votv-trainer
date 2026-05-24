#pragma once

namespace votv::config {

struct TrainerConfig {
    bool show_debug_tab = true;
    bool warn_about_saves = true;
};

TrainerConfig& current();

} // namespace votv::config
