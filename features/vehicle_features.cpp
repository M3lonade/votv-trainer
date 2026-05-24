#include "features/features.hpp"

#include "ue/sdk_context.hpp"

#include <imgui.h>

#include <cfloat>

namespace votv::features {
namespace {

bool g_freeze_fuel = false;
bool g_freeze_battery = false;
bool g_freeze_health = false;
float g_fuel = 100.0f;
float g_battery = 100.0f;
float g_health = 100.0f;
float g_default_speed = 0.0f;
float g_turbo_speed = 0.0f;
bool g_values_initialized = false;

void refresh_vehicle_values()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.atv) {
        return;
    }

    g_fuel = ctx.atv->fuel;
    g_battery = ctx.atv->battery;
    g_health = ctx.atv->health;
    g_default_speed = ctx.atv->speed_default;
    g_turbo_speed = ctx.atv->speed_turbo;
    g_values_initialized = true;
}

void apply_vehicle_freezes()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.atv) {
        return;
    }

    if (g_freeze_fuel) {
        ctx.atv->fuel = g_fuel;
        ctx.atv->Empty = false;
    }
    if (g_freeze_battery) {
        ctx.atv->battery = g_battery;
    }
    if (g_freeze_health) {
        ctx.atv->health = g_health;
        ctx.atv->brokenn = false;
    }
}

} // namespace

void render_vehicle_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.atv) {
        ImGui::TextUnformatted("ATV not resolved yet.");
        return;
    }

    if (!g_values_initialized) {
        refresh_vehicle_values();
    }

    if (ImGui::Button("Refresh ATV Values")) {
        refresh_vehicle_values();
    }
    ImGui::SameLine();
    ImGui::Text("Current speed: %.2f", ctx.atv->Speed);

    if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("atv_resources", 3, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Freeze", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthFixed, 100.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Fuel", &g_freeze_fuel);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##fuel", &g_fuel, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.atv->fuel);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Battery", &g_freeze_battery);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##battery", &g_battery, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.atv->battery);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Health", &g_freeze_health);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##health", &g_health, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.atv->health);
            ImGui::EndTable();
        }
    }

    if (ImGui::Button("Repair ATV")) {
        g_health = 100.0f;
        ctx.atv->health = g_health;
        ctx.atv->brokenn = false;
        ctx.atv->Empty = false;
        if (ctx.save_slot) {
            ctx.save_slot->carHealth = g_health;
        }
    }

    if (ImGui::CollapsingHeader("Speed Tuning")) {
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputFloat("Default speed", &g_default_speed, 10.0f, 100.0f, "%.1f");
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputFloat("Turbo speed", &g_turbo_speed, 10.0f, 100.0f, "%.1f");
        if (ImGui::Button("Apply Speed")) {
            ctx.atv->speed_default = g_default_speed;
            ctx.atv->speed_turbo = g_turbo_speed;
        }
    }

}

void tick_vehicle_features()
{
    apply_vehicle_freezes();
}

} // namespace votv::features
