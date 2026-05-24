#include "features/features.hpp"

#include "SDK/Engine_classes.hpp"
#include "SDK/baseWindow_classes.hpp"
#include "SDK/dish_classes.hpp"
#include "SDK/generator_classes.hpp"
#include "SDK/sink_classes.hpp"
#include "ue/process_event_hook.hpp"
#include "ue/sdk_context.hpp"
#include "util/log.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <mutex>
#include <string>
#include <vector>

namespace votv::features {
namespace {

int g_points_delta = 100;
int g_points_set = 0;
float g_total_time = 0.0f;
float g_day = 0.0f;
float g_time_scale = 1.0f;
bool g_values_initialized = false;
std::vector<int> g_transformer_cycle_values;
int g_selected_dish = -1;
std::string g_cleanup_result;
std::vector<std::string> g_cleanup_diagnostics;
std::mutex g_cleanup_mutex;
int g_grime_count = -1;
int g_trash_count = -1;
int g_bio_count = -1;
constexpr int k_max_cleanup_destroy_per_action = 250;

const char* g_grime_classes[] = {
    "grime_C",
    "grime_arirGraffiti_C",
    "grime_beer_C",
    "grime_blood2_C",
    "grime_blood2_drip_C",
    "grime_coffee_C",
    "grime_crack_C",
    "grime_dusty_C",
    "grime_dyn_C",
    "grime_explosionScorch_C",
    "grime_fallenLeaves_C",
    "grime_gasoline_C",
    "grime_grainy_C",
    "grime_leaky_C",
    "grime_leaky_rusty_C",
    "grime_leaky_wet_C",
    "grime_light_C",
    "grime_oil_C",
    "grime_poo_C",
    "grime_uv_C",
    "grime_wine_C",
};

const char* g_trash_classes[] = {
    "trashBitsPile_C",
    "actorChipPile_C",
    "actorChipPile_erie_C",
    "actorChipPile_leaves_C",
    "actorChipPile_wetConcrete_C",
    "prop_dirtball_C",
    "prop_garbageClump_C",
    "prop_garbageClump_erie_C",
    "prop_garbageClump_leaves_C",
    "prop_garbageClump_wetConcrete_C",
    "prop_garbageBag_C",
    "prop_garbBagFold_C",
    "prop_garbBagRoll_C",
};

const char* g_bio_mess_classes[] = {
    "prop_poo_C",
    "prop_poo_Child_C",
    "prop_deadRoach_C",
    "singleRoach_C",
    "prop_cockroachNest_C",
    "prop_bloodGib_C",
    "prop_bloodGib_arm_C",
    "prop_bloodGib_leg_C",
    "prop_bloodGib_deer_C",
    "prop_bloodGib_deer_body_C",
    "prop_bloodGib_deer_leg_B_C",
    "prop_bloodGib_deer_leg_F_C",
    "prop_bloodGib_deer_leg_horn_C",
    "bloodClot_C",
    "blooder_C",
};

void refresh_world_values()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (ctx.save_slot) {
        g_points_set = ctx.save_slot->Points;
        g_total_time = ctx.save_slot->totalTime;
        g_day = ctx.save_slot->Day;
    }
    if (ctx.daynight) {
        g_total_time = ctx.daynight->totalTime;
        g_day = ctx.daynight->Day;
        g_time_scale = ctx.daynight->TimeScale;
    }
    g_values_initialized = true;
}

std::string dish_name(const SDK::AmainGamemode_C* gm, int index, const SDK::Adish_C* dish)
{
    if (dish && dish->techName) {
        const std::string tech = dish->techName.ToString();
        if (!tech.empty()) {
            return tech;
        }
    }

    if (gm && index >= 0 && index < gm->dishes_techNames.Num()) {
        const std::string tech = gm->dishes_techNames[index].ToString();
        if (!tech.empty()) {
            return tech;
        }
    }

    if (dish) {
        const std::string text = dish->nameDish.ToString();
        if (!text.empty()) {
            return text;
        }
    }

    return "Dish " + std::to_string(index);
}

int count_live_actors(const char* const* class_names, int class_count);

SDK::FTransform transform_near_actor(SDK::AActor* actor)
{
    SDK::FTransform transform{};
    if (!actor) {
        return transform;
    }

    transform.Translation = actor->K2_GetActorLocation();
    transform.Translation.X += 250.0f;
    transform.Translation.Z += 120.0f;
    transform.Scale3D = SDK::FVector(1.0f, 1.0f, 1.0f);
    return transform;
}

void set_cleanup_result(const std::string& text)
{
    {
        std::scoped_lock lock(g_cleanup_mutex);
        g_cleanup_result = text;
    }
    log::write_format("Cleanup: %s", text.c_str());
}

std::string cleanup_result_snapshot()
{
    std::scoped_lock lock(g_cleanup_mutex);
    return g_cleanup_result;
}

std::vector<std::string> cleanup_diagnostics_snapshot()
{
    std::scoped_lock lock(g_cleanup_mutex);
    return g_cleanup_diagnostics;
}

void update_cleanup_counts()
{
    const int grime = count_live_actors(g_grime_classes, IM_ARRAYSIZE(g_grime_classes));
    const int trash = count_live_actors(g_trash_classes, IM_ARRAYSIZE(g_trash_classes));
    const int bio = count_live_actors(g_bio_mess_classes, IM_ARRAYSIZE(g_bio_mess_classes));

    std::scoped_lock lock(g_cleanup_mutex);
    g_grime_count = grime;
    g_trash_count = trash;
    g_bio_count = bio;
}

int count_live_actors(const char* const* class_names, int class_count)
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.world) {
        return 0;
    }

    int total = 0;
    for (int i = 0; i < class_count; ++i) {
        SDK::UClass* actor_class = SDK::UObject::FindClassFast(class_names[i]);
        if (!actor_class) {
            continue;
        }

        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(ctx.world, actor_class, &actors);
        total += actors.Num();
    }
    return total;
}

int destroy_live_actors(const char* const* class_names, int class_count, int max_destroy = k_max_cleanup_destroy_per_action)
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.world) {
        set_cleanup_result("World is not ready for live cleanup.");
        return 0;
    }

    int destroyed = 0;
    for (int i = 0; i < class_count; ++i) {
        SDK::UClass* actor_class = SDK::UObject::FindClassFast(class_names[i]);
        if (!actor_class) {
            log::write_format("Cleanup class not found: %s", class_names[i]);
            continue;
        }

        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(ctx.world, actor_class, &actors);
        for (int actor_index = 0; actor_index < actors.Num(); ++actor_index) {
            SDK::AActor* actor = actors[actor_index];
            if (!actor) {
                continue;
            }
            actor->K2_DestroyActor();
            ++destroyed;
            if (destroyed >= max_destroy) {
                return destroyed;
            }
        }
    }
    return destroyed;
}

void destroy_group(const char* label, const char* const* class_names, int class_count)
{
    const int destroyed = destroy_live_actors(class_names, class_count);
    set_cleanup_result(std::string("Destroyed ") + std::to_string(destroyed) + " live " + label + " actor(s). Refresh counts and press again if any remain.");
}

bool contains_cleanup_word(const std::string& value)
{
    const std::string lower = [&]() {
        std::string copy = value;
        std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return copy;
    }();

    return lower.find("trash") != std::string::npos ||
        lower.find("garb") != std::string::npos ||
        lower.find("pile") != std::string::npos ||
        lower.find("clump") != std::string::npos ||
        lower.find("chip") != std::string::npos ||
        lower.find("dirt") != std::string::npos ||
        lower.find("debris") != std::string::npos;
}

void refresh_cleanup_diagnostics()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    std::vector<std::string> diagnostics;
    if (!ctx.world) {
        diagnostics.push_back("World is not ready.");
        std::scoped_lock lock(g_cleanup_mutex);
        g_cleanup_diagnostics = std::move(diagnostics);
        return;
    }

    SDK::TArray<SDK::AActor*> actors;
    SDK::UGameplayStatics::GetAllActorsOfClass(ctx.world, SDK::AActor::StaticClass(), &actors);

    std::vector<std::pair<std::string, int>> counts;
    for (int i = 0; i < actors.Num(); ++i) {
        SDK::AActor* actor = actors[i];
        if (!actor || !actor->Class) {
            continue;
        }

        const std::string class_name = actor->Class->GetName();
        if (!contains_cleanup_word(class_name)) {
            continue;
        }

        auto existing = std::find_if(counts.begin(), counts.end(), [&](const auto& entry) {
            return entry.first == class_name;
        });
        if (existing != counts.end()) {
            ++existing->second;
        } else {
            counts.push_back({ class_name, 1 });
        }
    }

    std::sort(counts.begin(), counts.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& [class_name, count] : counts) {
        diagnostics.push_back(class_name + ": " + std::to_string(count));
    }
    if (diagnostics.empty()) {
        diagnostics.push_back("No trash-like live actor class names found.");
    }

    std::scoped_lock lock(g_cleanup_mutex);
    g_cleanup_diagnostics = std::move(diagnostics);
}

int clean_sinks()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.world) {
        return 0;
    }

    SDK::TArray<SDK::AActor*> actors;
    SDK::UGameplayStatics::GetAllActorsOfClass(ctx.world, SDK::Asink_C::StaticClass(), &actors);

    int cleaned = 0;
    for (int i = 0; i < actors.Num(); ++i) {
        auto* sink = static_cast<SDK::Asink_C*>(actors[i]);
        if (!sink) {
            continue;
        }
        sink->clean = 100000.0f;
        sink->upd();
        ++cleaned;
    }
    return cleaned;
}

int clean_windows()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.world) {
        return 0;
    }

    SDK::TArray<SDK::AActor*> actors;
    SDK::UGameplayStatics::GetAllActorsOfClass(ctx.world, SDK::AbaseWindow_C::StaticClass(), &actors);

    int cleaned = 0;
    for (int i = 0; i < actors.Num(); ++i) {
        auto* window = static_cast<SDK::AbaseWindow_C*>(actors[i]);
        if (!window) {
            continue;
        }
        window->clean = 100000.0f;
        window->setClean();
        ++cleaned;
    }
    return cleaned;
}

} // namespace

void render_world_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.game_mode) {
        ImGui::TextUnformatted("Game mode not resolved yet.");
        return;
    }

    if (!g_values_initialized) {
        refresh_world_values();
    }

    if (ImGui::Button("Refresh World Values")) {
        refresh_world_values();
    }

    if (ImGui::BeginTable("world_controls", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::BeginChild("points_panel", ImVec2(0.0f, 150.0f), true);
        ImGui::SeparatorText("Points");
        if (ctx.save_slot) {
            ImGui::Text("Current: %d", ctx.save_slot->Points);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputInt("Add amount", &g_points_delta);
            if (ImGui::Button("Add Points")) {
                ctx.game_mode->AddPoints(g_points_delta);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Set", &g_points_set);
            ImGui::SameLine();
            if (ImGui::Button("Apply")) {
                ctx.save_slot->Points = g_points_set;
            }
        } else {
            ImGui::TextUnformatted("Save slot not resolved yet.");
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("time_panel", ImVec2(0.0f, 150.0f), true);
        ImGui::SeparatorText("Time");
        if (ctx.daynight) {
            ImGui::Text("Day %.2f | Total %.2f", ctx.daynight->Day, ctx.daynight->totalTime);
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputFloat("Day", &g_day, 1.0f, 10.0f, "%.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputFloat("Scale", &g_time_scale, 0.1f, 1.0f, "%.2f");
            ImGui::SetNextItemWidth(180.0f);
            ImGui::InputFloat("Total time", &g_total_time, 60.0f, 600.0f, "%.2f");
            ImGui::SameLine();
            if (ImGui::Button("Apply Time")) {
                ctx.daynight->totalTime = g_total_time;
                ctx.daynight->Day = g_day;
                ctx.daynight->TimeScale = g_time_scale;
                if (ctx.save_slot) {
                    ctx.save_slot->totalTime = g_total_time;
                    ctx.save_slot->Day = g_day;
                }
            }
        } else {
            ImGui::TextUnformatted("Day/night cycle not resolved yet.");
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }

}

void render_world_tools_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.game_mode) {
        ImGui::TextUnformatted("Game mode not resolved yet.");
        return;
    }

    if (ImGui::CollapsingHeader("Base Cleanup", ImGuiTreeNodeFlags_DefaultOpen)) {
        int grime_count = -1;
        int trash_count = -1;
        int bio_count = -1;
        {
            std::scoped_lock lock(g_cleanup_mutex);
            grime_count = g_grime_count;
            trash_count = g_trash_count;
            bio_count = g_bio_count;
        }

        ImGui::Text("Detected: grime %d | trash %d | bio/roaches %d", grime_count, trash_count, bio_count);
        if (ImGui::Button("Remove All Live Mess")) {
            ue::process_event_hook::enqueue_game_thread([] {
                const int grime = destroy_live_actors(g_grime_classes, IM_ARRAYSIZE(g_grime_classes));
                const int trash = destroy_live_actors(g_trash_classes, IM_ARRAYSIZE(g_trash_classes));
                const int bio = destroy_live_actors(g_bio_mess_classes, IM_ARRAYSIZE(g_bio_mess_classes));
                const int sinks = clean_sinks();
                const int windows = clean_windows();
                set_cleanup_result("Destroyed " + std::to_string(grime + trash + bio) + " live mess actor(s), cleaned " + std::to_string(sinks + windows) + " fixture(s).");
                update_cleanup_counts();
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh Counts")) {
            ue::process_event_hook::enqueue_game_thread([] {
                update_cleanup_counts();
                set_cleanup_result("Cleanup counts refreshed.");
            });
        }

        if (ImGui::CollapsingHeader("Cleanup Details")) {
            if (ImGui::Button("Remove Grime/Stains")) {
                ue::process_event_hook::enqueue_game_thread([] {
                    destroy_group("grime/stain", g_grime_classes, IM_ARRAYSIZE(g_grime_classes));
                    update_cleanup_counts();
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Trash Piles")) {
                ue::process_event_hook::enqueue_game_thread([] {
                    destroy_group("trash", g_trash_classes, IM_ARRAYSIZE(g_trash_classes));
                    update_cleanup_counts();
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Bio Mess/Roaches")) {
                ue::process_event_hook::enqueue_game_thread([] {
                    destroy_group("bio mess", g_bio_mess_classes, IM_ARRAYSIZE(g_bio_mess_classes));
                    update_cleanup_counts();
                });
            }

            if (ImGui::Button("Clean Sinks/Fixtures")) {
                ue::process_event_hook::enqueue_game_thread([] {
                    const int sinks = clean_sinks();
                    const int windows = clean_windows();
                    set_cleanup_result("Cleaned " + std::to_string(sinks) + " sink(s)/fixture(s) and " + std::to_string(windows) + " window(s).");
                });
            }
        }

        const std::string cleanup_result = cleanup_result_snapshot();
        if (!cleanup_result.empty()) {
            ImGui::TextWrapped("%s", cleanup_result.c_str());
        }

        if (ImGui::CollapsingHeader("Diagnostics")) {
            if (ImGui::Button("Scan Trash-Like Actor Classes")) {
                ue::process_event_hook::enqueue_game_thread([] {
                    refresh_cleanup_diagnostics();
                });
            }
            for (const std::string& line : cleanup_diagnostics_snapshot()) {
                ImGui::TextUnformatted(line.c_str());
            }
        }
    }

    if (ImGui::CollapsingHeader("Transformers")) {
        const int count = ctx.game_mode->Generators.Num();
        if (static_cast<int>(g_transformer_cycle_values.size()) != count) {
            g_transformer_cycle_values.assign(count, 0);
            for (int i = 0; i < count; ++i) {
                if (ctx.game_mode->Generators[i]) {
                    g_transformer_cycle_values[i] = ctx.game_mode->Generators[i]->cycle;
                }
            }
        }

        ImGui::Text("Found %d transformer(s)", count);
        if (ImGui::BeginTable("transformers_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Target");
            ImGui::TableSetupColumn("Broken");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            for (int i = 0; i < count; ++i) {
                SDK::Agenerator_C* generator = ctx.game_mode->Generators[i];
                if (!generator) {
                    continue;
                }

                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Transformer %d", i + 1);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", generator->cycle);
                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(80.0f);
                ImGui::InputInt("##state", &g_transformer_cycle_values[i]);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(generator->IsBroken ? "yes" : "no");
                ImGui::TableSetColumnIndex(4);
                if (ImGui::SmallButton("Apply")) {
                    generator->cycle = g_transformer_cycle_values[i];
                    generator->IsBroken = false;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Fix")) {
                    generator->IsBroken = false;
                    generator->cycle = g_transformer_cycle_values[i];
                    generator->fullFix();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Break")) {
                    generator->IsBroken = true;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Active Event Monitor")) {
        ImGui::Text("Active event count: %d", ctx.game_mode->activeEvents);
        ImGui::Text("Disable event track: %d", ctx.game_mode->disableEventTrack);
        ImGui::Text("Active flags: %d", ctx.game_mode->eventsActive.Num());
        ImGui::Text("Active sender objects: %d", ctx.game_mode->activeEvents_senders.Num());

        if (ImGui::BeginTable("active_event_senders", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("Sender");
            ImGui::TableHeadersRow();
            for (int i = 0; i < ctx.game_mode->activeEvents_senders.Num(); ++i) {
                SDK::UObject* sender = ctx.game_mode->activeEvents_senders[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s  0x%p", sender ? sender->GetFullName().c_str() : "<null>", sender);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Satellite Dishes")) {
        const int count = ctx.game_mode->dishs.Num();
        ImGui::Text("Found %d dish(es)", count);

        if (ImGui::BeginTable("dish_picker", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("dish_list", ImVec2(0.0f, 230.0f), true);
            for (int i = 0; i < count; ++i) {
                SDK::Adish_C* dish = ctx.game_mode->dishs[i];
                if (!dish) {
                    continue;
                }

                const std::string label = dish_name(ctx.game_mode, i, dish);
                if (ImGui::Selectable(label.c_str(), g_selected_dish == i)) {
                    g_selected_dish = i;
                }
            }
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("dish_details", ImVec2(0.0f, 230.0f), true);
            ImGui::SeparatorText("Selected Dish");
            if (g_selected_dish >= 0 && g_selected_dish < count && ctx.game_mode->dishs[g_selected_dish]) {
                SDK::Adish_C* dish = ctx.game_mode->dishs[g_selected_dish];
                const SDK::FVector loc = dish->K2_GetActorLocation();
                ImGui::TextWrapped("%s", dish_name(ctx.game_mode, g_selected_dish, dish).c_str());
                ImGui::Text("X %.1f", loc.X);
                ImGui::Text("Y %.1f", loc.Y);
                ImGui::Text("Z %.1f", loc.Z);
                if (ImGui::Button("Teleport To Selected Dish")) {
                    ctx.game_mode->transformToPlayer(transform_near_actor(dish));
                }
            } else {
                ImGui::TextDisabled("Select a dish.");
            }
            ImGui::EndChild();
            ImGui::EndTable();
        }
    }
}

void render_debug_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    ImGui::Text("World:      0x%p", ctx.world);
    ImGui::Text("GameMode:   0x%p", ctx.game_mode);
    ImGui::Text("Player:     0x%p", ctx.player);
    ImGui::Text("SaveSlot:   0x%p", ctx.save_slot);
    ImGui::Text("ATV:        0x%p", ctx.atv);
    ImGui::Text("DayNight:   0x%p", ctx.daynight);

    if (ctx.player) {
        ImGui::Separator();
        ImGui::Text("Player air: %.2f", ctx.player->air);
        ImGui::Text("Food drain: %.2f", ctx.player->foodDraining);
        ImGui::Text("Sleep drain: %.2f", ctx.player->sleepDraining);
    }
}

} // namespace votv::features
