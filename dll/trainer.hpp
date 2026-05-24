#pragma once

#include <atomic>

namespace votv {

class Trainer {
public:
    static Trainer& instance();

    void initialize();
    void shutdown();
    void tick();
    void render_menu();

    void toggle_menu();
    bool menu_open() const;

private:
    std::atomic_bool initialized_ = false;
    std::atomic_bool menu_open_ = true;
};

namespace trainer {
Trainer& instance();
}

} // namespace votv
