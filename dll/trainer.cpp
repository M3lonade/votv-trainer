#include "dll/trainer.hpp"

#include "config/config.hpp"
#include "features/features.hpp"
#include "render/theme.hpp"
#include "ue/process_event_hook.hpp"
#include "ue/sdk_context.hpp"
#include "util/log.hpp"
#include "util/win32.hpp"

#include <imgui.h>

namespace votv {

Trainer& Trainer::instance()
{
    static Trainer trainer;
    return trainer;
}

void Trainer::initialize()
{
    if (initialized_.load()) {
        return;
    }

    ue::SdkContext::instance().initialize();
    ue::process_event_hook::install(false);
    initialized_.store(true);
    log::write("Trainer initialized");
}

void Trainer::shutdown()
{
    initialized_.store(false);
}

void Trainer::tick()
{
    if (!initialized_.load()) {
        return;
    }

    ue::SdkContext::instance().update();
    features::tick_all();
}

void Trainer::render_menu()
{
    bool open = menu_open_.load();
    if (!open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("VotV Trainer", &open, ImGuiWindowFlags_NoCollapse)) {
        menu_open_.store(open);
        ImGui::End();
        return;
    }
    menu_open_.store(open);

    render::theme::draw_space_window_background();

    const auto& context = ue::SdkContext::instance();
    ImGui::TextColored(ImVec4(0.42f, 0.82f, 1.0f, 1.0f), "Status: %s", context.status().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Insert: menu | End: unload");
    ImGui::SameLine(ImGui::GetWindowWidth() - 170.0f);
    if (ImGui::SmallButton("Options")) {
        ImGui::OpenPopup("trainer_header_options");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Unload Trainer")) {
        win32::request_unload();
    }

    if (config::current().warn_about_saves) {
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "Save-affecting options: back up saves first.");
    }

    if (ImGui::BeginPopup("trainer_header_options")) {
        bool pe_logging = ue::process_event_hook::logging_enabled();
        if (ImGui::Checkbox("ProcessEvent logging", &pe_logging)) {
            ue::process_event_hook::set_logging(pe_logging);
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginTabBar("trainer_tabs")) {
        if (ImGui::BeginTabItem("Player")) {
            features::render_player_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("World")) {
            features::render_world_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ATV")) {
            features::render_vehicle_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("World Tools")) {
            features::render_world_tools_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Item Spawner")) {
            features::render_item_spawner_tab();
            ImGui::EndTabItem();
        }
        if (config::current().show_debug_tab && ImGui::BeginTabItem("Debug")) {
            features::render_debug_tab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Trainer::toggle_menu()
{
    menu_open_.store(!menu_open_.load());
}

bool Trainer::menu_open() const
{
    return menu_open_.load();
}

namespace trainer {

Trainer& instance()
{
    return Trainer::instance();
}

} // namespace trainer

} // namespace votv
