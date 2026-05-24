#pragma once

#include "SDK/ATV_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/analogDScreenTest_classes.hpp"
#include "SDK/blackFog_classes.hpp"
#include "SDK/badSun_classes.hpp"
#include "SDK/daynightCycle_classes.hpp"
#include "SDK/lakeglow_classes.hpp"
#include "SDK/mainGamemode_classes.hpp"
#include "SDK/mainPlayer_classes.hpp"
#include "SDK/redSkyEvent_classes.hpp"
#include "SDK/saveSlot_classes.hpp"
#include "SDK/trigger_eventer_classes.hpp"

#include <string>

namespace votv::ue {

struct ContextSnapshot {
    SDK::UWorld* world = nullptr;
    SDK::AmainGamemode_C* game_mode = nullptr;
    SDK::AmainPlayer_C* player = nullptr;
    SDK::UsaveSlot_C* save_slot = nullptr;
    SDK::AATV_C* atv = nullptr;
    SDK::AdaynightCycle_C* daynight = nullptr;
    SDK::AanalogDScreenTest_C* signal_panel = nullptr;
    SDK::Atrigger_eventer_C* eventer = nullptr;
    SDK::UDataTable* events_table = nullptr;
    SDK::AblackFog_C* black_fog = nullptr;
    SDK::AredSkyEvent_C* red_sky = nullptr;
    SDK::AbadSun_C* bad_sun = nullptr;
    SDK::Alakeglow_C* lake_glow = nullptr;
};

class SdkContext {
public:
    static SdkContext& instance();

    void initialize();
    void update();

    const ContextSnapshot& snapshot() const;
    bool ready() const;
    std::string status() const;

private:
    SdkContext() = default;

    ContextSnapshot snapshot_{};
    bool initialized_ = false;
    std::string status_ = "Not initialized";
};

} // namespace votv::ue
