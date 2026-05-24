#include "features/features.hpp"

#include "ue/sdk_context.hpp"

#include <imgui.h>

#include <cfloat>

namespace votv::features {
namespace {

struct NoclipRestoreState {
    bool captured = false;
    float gravity_scale = 1.0f;
    float max_fly_speed = 0.0f;
    float max_walk_speed = 0.0f;
    SDK::EMovementMode movement_mode = SDK::EMovementMode::MOVE_Walking;
    SDK::ECollisionEnabled capsule_collision = SDK::ECollisionEnabled::QueryAndPhysics;
};

bool g_freeze_health = false;
bool g_freeze_food = false;
bool g_freeze_sleep = false;
bool g_freeze_air = false;
bool g_freeze_flashlight_battery = false;
bool g_freeze_strength_agility = false;
bool g_true_noclip = false;
bool g_no_survival_drain = false;
bool g_values_initialized = false;
float g_health_value = 100.0f;
float g_food_value = 100.0f;
float g_sleep_value = 100.0f;
float g_air_value = 100.0f;
float g_flashlight_battery_value = 100.0f;
float g_strength_value = 1.0f;
float g_agility_value = 1.0f;
float g_noclip_speed = 1800.0f;
NoclipRestoreState g_noclip_restore;

void refresh_player_values()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (ctx.save_slot) {
        g_health_value = ctx.save_slot->health;
        g_food_value = ctx.save_slot->food;
        g_sleep_value = ctx.save_slot->sleep;
        g_flashlight_battery_value = ctx.save_slot->battery;
        g_strength_value = ctx.save_slot->Strength != 0.0f ? ctx.save_slot->Strength : (ctx.player ? ctx.player->Str : 0.0f);
        g_agility_value = ctx.save_slot->agility != 0.0f ? ctx.save_slot->agility : (ctx.player ? ctx.player->agil : 0.0f);
    }
    if (ctx.player) {
        g_air_value = ctx.player->air;
    }
    g_values_initialized = true;
}

void apply_player_freezes()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.player) {
        return;
    }

    if (g_true_noclip) {
        auto* movement = ctx.player->CharacterMovement;
        if (movement) {
            if (!g_noclip_restore.captured) {
                g_noclip_restore.gravity_scale = movement->GravityScale;
                g_noclip_restore.max_fly_speed = movement->MaxFlySpeed;
                g_noclip_restore.max_walk_speed = movement->MaxWalkSpeed;
                g_noclip_restore.movement_mode = movement->MovementMode;
                if (ctx.player->CapsuleComponent) {
                    g_noclip_restore.capsule_collision = ctx.player->CapsuleComponent->GetCollisionEnabled();
                }
                g_noclip_restore.captured = true;
            }

            movement->GravityScale = 0.0f;
            movement->MaxFlySpeed = g_noclip_speed;
            movement->MaxWalkSpeed = g_noclip_speed;
            movement->MovementMode = SDK::EMovementMode::MOVE_Flying;
            movement->Velocity.Z = 0.0f;
            movement->PendingLaunchVelocity = SDK::FVector(0.0f, 0.0f, 0.0f);
        }

        ctx.player->noclip = true;
        ctx.player->SetActorEnableCollision(false);
        if (ctx.player->CapsuleComponent) {
            ctx.player->CapsuleComponent->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);
            ctx.player->CapsuleComponent->SetCollisionResponseToAllChannels(SDK::ECollisionResponse::ECR_Ignore);
        }
        ctx.player->fallVeloc = SDK::FVector(0.0f, 0.0f, 0.0f);
    } else if (g_noclip_restore.captured) {
        auto* movement = ctx.player->CharacterMovement;
        if (movement) {
            movement->GravityScale = g_noclip_restore.gravity_scale;
            movement->MaxFlySpeed = g_noclip_restore.max_fly_speed;
            movement->MaxWalkSpeed = g_noclip_restore.max_walk_speed;
            movement->MovementMode = g_noclip_restore.movement_mode;
        }

        ctx.player->noclip = false;
        ctx.player->SetActorEnableCollision(true);
        if (ctx.player->CapsuleComponent) {
            ctx.player->CapsuleComponent->SetCollisionEnabled(g_noclip_restore.capsule_collision);
            ctx.player->CapsuleComponent->SetCollisionResponseToAllChannels(SDK::ECollisionResponse::ECR_Block);
        }
        g_noclip_restore = {};
    }

    if (g_freeze_air) {
        ctx.player->air = g_air_value;
    }

    if (g_no_survival_drain) {
        ctx.player->foodDraining = 0.0f;
        ctx.player->sleepDraining = 0.0f;
        ctx.player->skipFatigue = true;
    }

    if (g_freeze_strength_agility) {
        ctx.player->Str = g_strength_value;
        ctx.player->agil = g_agility_value;
    }

    if (!ctx.save_slot) {
        return;
    }

    if (g_freeze_health) {
        ctx.save_slot->health = g_health_value;
        ctx.save_slot->p_health = g_health_value;
    }
    if (g_freeze_food) {
        ctx.save_slot->food = g_food_value;
    }
    if (g_freeze_sleep) {
        ctx.save_slot->sleep = g_sleep_value;
    }
    if (g_freeze_flashlight_battery) {
        ctx.save_slot->battery = g_flashlight_battery_value;
    }
    if (g_freeze_strength_agility) {
        ctx.save_slot->Strength = g_strength_value;
        ctx.save_slot->agility = g_agility_value;
    }
}

} // namespace

void tick_player_features()
{
    apply_player_freezes();
}

void render_player_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.player) {
        ImGui::TextUnformatted("Player not resolved yet.");
        return;
    }

    if (!g_values_initialized) {
        refresh_player_values();
    }

    if (ImGui::Button("Refresh Player Values")) {
        refresh_player_values();
    }
    ImGui::SameLine();
    if (ImGui::Button("Heal +25")) {
        ctx.player->heal(25.0f);
    }

    if (!ctx.save_slot) {
        ImGui::TextUnformatted("Save slot not resolved yet.");
    }

    if (ctx.save_slot && ImGui::CollapsingHeader("Survival", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("player_survival", 3, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Freeze", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthFixed, 110.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Health", &g_freeze_health);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##health", &g_health_value, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.save_slot->health);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Food", &g_freeze_food);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##food", &g_food_value, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.save_slot->food);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Sleep", &g_freeze_sleep);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##sleep", &g_sleep_value, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.save_slot->sleep);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Air", &g_freeze_air);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##air", &g_air_value, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.player->air);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("Battery", &g_freeze_flashlight_battery);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputFloat("##flashlight_battery", &g_flashlight_battery_value, 1.0f, 10.0f, "%.1f");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", ctx.save_slot->battery);
            ImGui::EndTable();
        }

        ImGui::Checkbox("No food/sleep drain", &g_no_survival_drain);
        ImGui::SameLine();
        if (ImGui::Button("Apply Stats")) {
            ctx.save_slot->health = g_health_value;
            ctx.save_slot->p_health = g_health_value;
            ctx.save_slot->food = g_food_value;
            ctx.save_slot->sleep = g_sleep_value;
        }
    }

    if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Noclip", &g_true_noclip);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.0f);
        ImGui::InputFloat("Speed", &g_noclip_speed, 100.0f, 500.0f, "%.0f");

        ImGui::Checkbox("Can fall damage", &ctx.player->canFallDamage);
        ImGui::SameLine();
        ImGui::Checkbox("Can ragdoll", &ctx.player->canRagdoll);
        ImGui::SameLine();
        ImGui::Checkbox("Start invincibility flag", &ctx.player->startInvinc);
    }

    if (ctx.save_slot && ImGui::CollapsingHeader("Attributes")) {
        ImGui::Text("Live: strength %.2f / agility %.2f", ctx.player->Str, ctx.player->agil);
        ImGui::Checkbox("Keep strength/agility applied", &g_freeze_strength_agility);
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputFloat("Strength", &g_strength_value, 0.25f, 1.0f, "%.2f");
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputFloat("Agility", &g_agility_value, 0.25f, 1.0f, "%.2f");
        if (ImGui::Button("Apply Strength/Agility")) {
            ctx.save_slot->Strength = g_strength_value;
            ctx.save_slot->agility = g_agility_value;
            ctx.player->Str = g_strength_value;
            ctx.player->agil = g_agility_value;
            ctx.player->updateSpeed();
        }
    }
}

} // namespace votv::features
