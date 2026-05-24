#include "ue/sdk_context.hpp"

#include "util/log.hpp"
#include "util/memory.hpp"

namespace votv::ue {
namespace {

SDK::UDataTable* resolve_events_table()
{
    if (auto* table = SDK::UObject::FindObject<SDK::UDataTable>("DataTable list_events.list_events")) {
        return table;
    }

    return SDK::UObject::FindObjectFast<SDK::UDataTable>("list_events");
}

} // namespace

SdkContext& SdkContext::instance()
{
    static SdkContext context;
    return context;
}

void SdkContext::initialize()
{
    initialized_ = true;
    status_ = "SDK context initialized";
    log::write_format("Game base: 0x%p", reinterpret_cast<void*>(memory::module_base()));
    log::write_format("GWorld: 0x%p", reinterpret_cast<void*>(memory::absolute(SDK::Offsets::GWorld)));
    log::write_format("GObjects: 0x%p", reinterpret_cast<void*>(memory::absolute(SDK::Offsets::GObjects)));
}

void SdkContext::update()
{
    if (!initialized_) {
        initialize();
    }

    ContextSnapshot next{};

    next.world = SDK::UWorld::GetWorld();
    if (!next.world) {
        status_ = "Waiting for UWorld";
        snapshot_ = next;
        return;
    }

    if (next.world->AuthorityGameMode) {
        next.game_mode = static_cast<SDK::AmainGamemode_C*>(next.world->AuthorityGameMode);
    }

    if (next.game_mode) {
        next.player = next.game_mode->mainPlayer;
        next.save_slot = next.game_mode->saveSlot;
        next.atv = next.game_mode->car;
        next.daynight = next.game_mode->daynightCycle;
        next.eventer = next.game_mode->eventer;
        next.events_table = resolve_events_table();
        next.black_fog = next.game_mode->blackFog;
        next.red_sky = next.game_mode->redSky;
        next.bad_sun = next.game_mode->badSun;
        next.lake_glow = next.game_mode->event_lakeglow;
    }

    if (next.player && !next.atv) {
        next.atv = next.player->ATV;
    }

    status_ = next.game_mode && next.player ? "Ready" : "World loaded, waiting for player";

    snapshot_ = next;
}

const ContextSnapshot& SdkContext::snapshot() const
{
    return snapshot_;
}

bool SdkContext::ready() const
{
    return snapshot_.world && snapshot_.game_mode && snapshot_.player;
}

std::string SdkContext::status() const
{
    return status_;
}

} // namespace votv::ue
