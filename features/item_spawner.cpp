#include "features/features.hpp"

#include "SDK/Engine_classes.hpp"
#include "SDK/propProcessor_classes.hpp"
#include "SDK/struct_prop_structs.hpp"
#include "ue/object_helpers.hpp"
#include "ue/sdk_context.hpp"
#include "util/log.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <string>
#include <vector>

namespace votv::features {
namespace {

struct ItemEntry {
    SDK::FName name{};
    std::string display;
    std::string internal_name;
    std::string description;
    std::string category;
    std::string tag;
    std::string class_name;
    int sdk_category = -1;
    int price = 0;
    bool is_food = false;
    bool can_hold = false;
    bool hidden = false;
    bool spoiler = false;
};

std::vector<ItemEntry> g_items;
char g_filter[128]{};
int g_selected = -1;
int g_selected_category = 0;
int g_amount = 1;
float g_spawn_offset = 160.0f;
bool g_send_to_inventory = false;
bool g_show_hidden = false;
bool g_show_spoilers = false;
std::string g_last_result;

const char* g_categories[] = {
    "All",
    "Food",
    "Tools",
    "Cleanup",
    "Containers",
    "Electronics",
    "Signal/Drives",
    "Furniture",
    "Decor",
    "Crafting",
    "Creatures/Event",
    "Other",
};

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool contains_text(const std::string& value, const char* needle)
{
    return lowercase(value).find(needle) != std::string::npos;
}

std::string text_to_string(const SDK::FText& text)
{
    return text.TextData ? text.ToString() : "";
}

std::string infer_category(const std::string& name, bool is_food)
{
    if (is_food || contains_text(name, "food") || contains_text(name, "burger") || contains_text(name, "mre") || contains_text(name, "drink")) {
        return "Food";
    }
    if (contains_text(name, "sponge") || contains_text(name, "mop") || contains_text(name, "broom") || contains_text(name, "trash") || contains_text(name, "garbage") || contains_text(name, "poo") || contains_text(name, "roach") || contains_text(name, "dirt")) {
        return "Cleanup";
    }
    if (contains_text(name, "tool") || contains_text(name, "wrench") || contains_text(name, "crowbar") || contains_text(name, "hook") || contains_text(name, "rake") || contains_text(name, "shovel")) {
        return "Tools";
    }
    if (contains_text(name, "box") || contains_text(name, "container") || contains_text(name, "crate") || contains_text(name, "bucket") || contains_text(name, "bin") || contains_text(name, "bag")) {
        return "Containers";
    }
    if (contains_text(name, "drive") || contains_text(name, "signal") || contains_text(name, "disk")) {
        return "Signal/Drives";
    }
    if (contains_text(name, "server") || contains_text(name, "radio") || contains_text(name, "computer") || contains_text(name, "battery") || contains_text(name, "lamp") || contains_text(name, "light") || contains_text(name, "drone") || contains_text(name, "kerfur")) {
        return "Electronics";
    }
    if (contains_text(name, "chair") || contains_text(name, "table") || contains_text(name, "bed") || contains_text(name, "shelf") || contains_text(name, "locker") || contains_text(name, "cabinet")) {
        return "Furniture";
    }
    if (contains_text(name, "poster") || contains_text(name, "plush") || contains_text(name, "rug") || contains_text(name, "plant") || contains_text(name, "picture") || contains_text(name, "decor")) {
        return "Decor";
    }
    if (contains_text(name, "metal") || contains_text(name, "scrap") || contains_text(name, "wood") || contains_text(name, "plank") || contains_text(name, "nail") || contains_text(name, "craft")) {
        return "Crafting";
    }
    if (contains_text(name, "alien") || contains_text(name, "arir") || contains_text(name, "ufo") || contains_text(name, "event") || contains_text(name, "wisp") || contains_text(name, "mann")) {
        return "Creatures/Event";
    }
    return "Other";
}

std::string category_tag(const std::string& category)
{
    if (category == "Food") {
        return "[FOOD]";
    }
    if (category == "Tools") {
        return "[TOOL]";
    }
    if (category == "Cleanup") {
        return "[CLEAN]";
    }
    if (category == "Containers") {
        return "[BOX]";
    }
    if (category == "Electronics") {
        return "[ELEC]";
    }
    if (category == "Signal/Drives") {
        return "[SIG]";
    }
    if (category == "Furniture") {
        return "[FURN]";
    }
    if (category == "Decor") {
        return "[DECOR]";
    }
    if (category == "Crafting") {
        return "[CRAFT]";
    }
    if (category == "Creatures/Event") {
        return "[EVENT]";
    }
    return "[ITEM]";
}

bool category_visible(const ItemEntry& entry)
{
    return g_selected_category == 0 || entry.category == g_categories[g_selected_category];
}

SDK::UDataTable* resolve_props_table()
{
    if (auto* table = SDK::UObject::FindObject<SDK::UDataTable>("DataTable list_props.list_props")) {
        return table;
    }

    return SDK::UObject::FindObjectFast<SDK::UDataTable>("list_props");
}

const SDK::Fstruct_prop* prop_table_row(const SDK::UDataTable* table, const SDK::FName& row_name)
{
    if (!table) {
        return nullptr;
    }

    for (const auto& pair : table->RowMap) {
        if (pair.Key() == row_name && pair.Value()) {
            return reinterpret_cast<const SDK::Fstruct_prop*>(pair.Value());
        }
    }
    return nullptr;
}

void add_item_entry(const SDK::FName& name, const SDK::Fstruct_prop& prop)
{
    if (name.IsNone()) {
        return;
    }

    const std::string internal_name = ue::name_to_string(name);
    if (internal_name.empty() || internal_name == "None") {
        return;
    }

    const auto exists = std::any_of(g_items.begin(), g_items.end(), [&](const ItemEntry& entry) {
        return entry.name == name || entry.internal_name == internal_name;
    });
    if (exists) {
        return;
    }

    ItemEntry entry{};
    entry.name = name;
    entry.internal_name = internal_name;
    entry.display = text_to_string(prop.displayName_8_FE83ADBF40AA162942FCE589F5806DD2);
    entry.description = text_to_string(prop.description_19_1CC55D5A4DAE9AE1A6DE5AA74C0792C6);
    entry.sdk_category = static_cast<int>(prop.category_42_B50473484322DA921629D9BE91DB63EC);
    entry.price = prop.price_27_397ABECF466FF5CA25FEBCBD300108FB;
    entry.can_hold = prop.canHold_58_FDF8EBA046BBA85ED9214BA09F10279A;
    entry.hidden = prop.hidden_34_9AD495D641D283C4EB3861BB6F4DEB65;
    entry.spoiler = prop.spoiler_39_95DAD306435E10741C46BB9FE4FBEE05;
    entry.class_name = prop.spawnAsObject_37_E2576B2F46591ED4C7C3B183E9F4D86B ? prop.spawnAsObject_37_E2576B2F46591ED4C7C3B183E9F4D86B->GetName() : "";

    if (entry.display.empty()) {
        entry.display = internal_name;
    }

    entry.is_food = contains_text(entry.internal_name, "food") || contains_text(entry.class_name, "food");
    entry.category = infer_category(entry.internal_name + " " + entry.display + " " + entry.class_name, entry.is_food);
    entry.tag = category_tag(entry.category);
    g_items.push_back(std::move(entry));
}

void rebuild_items(const SDK::AmainGamemode_C* gm)
{
    g_items.clear();
    if (!gm) {
        return;
    }

    if (gm->propRenderer) {
        const int count = std::min(gm->propRenderer->propsNames.Num(), gm->propRenderer->propData.Num());
        for (int i = 0; i < count; ++i) {
            add_item_entry(gm->propRenderer->propsNames[i], gm->propRenderer->propData[i]);
        }

        const int alt_count = std::min(gm->propRenderer->Names.Num(), gm->propRenderer->propData.Num());
        for (int i = 0; i < alt_count; ++i) {
            add_item_entry(gm->propRenderer->Names[i], gm->propRenderer->propData[i]);
        }
    }

    if (SDK::UDataTable* props_table = resolve_props_table()) {
        SDK::TArray<SDK::FName> row_names;
        SDK::UDataTableFunctionLibrary::GetDataTableRowNames(props_table, &row_names);
        for (int i = 0; i < row_names.Num(); ++i) {
            if (const SDK::Fstruct_prop* prop = prop_table_row(props_table, row_names[i])) {
                add_item_entry(row_names[i], *prop);
            }
        }
    }

    std::sort(g_items.begin(), g_items.end(), [](const ItemEntry& lhs, const ItemEntry& rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        return lhs.display < rhs.display;
    });

    if (g_selected >= static_cast<int>(g_items.size())) {
        g_selected = -1;
    }
}

void spawn_selected()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.game_mode || !ctx.player || g_selected < 0 || g_selected >= static_cast<int>(g_items.size())) {
        g_last_result = "Spawner is not ready.";
        return;
    }

    const auto& entry = g_items[g_selected];
    SDK::FTransform player_transform{};
    ctx.game_mode->playerPositionToTransform(&player_transform);
    SDK::FTransform spawn_transform = ue::make_spawn_transform(player_transform, g_spawn_offset);

    SDK::AActor* actor = nullptr;
    ctx.game_mode->spawnPropThroughGamemode(entry.name, spawn_transform, std::max(g_amount, 1), &actor);

    if (g_send_to_inventory && actor && ctx.player) {
        bool ok = false;
        ctx.player->putObjectInventory2(actor, false, &ok);
        g_last_result = ok ? "Spawned and sent item to inventory." : "Spawned item, but inventory insertion failed.";
    } else {
        g_last_result = actor ? "Spawned item near player." : "Spawn call returned no actor; check in-game.";
    }

    log::write_format("Spawner: %s x%d -> %s", entry.display.c_str(), g_amount, g_last_result.c_str());
}

} // namespace

void render_item_spawner_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (!ctx.game_mode) {
        ImGui::TextUnformatted("Game mode not resolved yet.");
        return;
    }

    if (ImGui::Button("Refresh Item List") || g_items.empty()) {
        rebuild_items(ctx.game_mode);
    }
    ImGui::SameLine();
    ImGui::Text("%zu items", g_items.size());
    if (g_items.empty()) {
        ImGui::TextWrapped("Item metadata is not ready yet. Wait until the game's prop processor finishes loading, then refresh.");
    }

    if (ImGui::BeginTable("spawner_controls", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Search", 0, 2.4f);
        ImGui::TableSetupColumn("Category", 0, 1.2f);
        ImGui::TableSetupColumn("Amount", 0, 0.7f);
        ImGui::TableSetupColumn("Offset", 0, 0.8f);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##Search", g_filter, sizeof(g_filter));
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::Combo("##Category", &g_selected_category, g_categories, IM_ARRAYSIZE(g_categories));
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##Amount", &g_amount);
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputFloat("##ForwardOffset", &g_spawn_offset, 10.0f, 50.0f, "%.0f");
        ImGui::EndTable();
    }
    g_amount = std::clamp(g_amount, 1, 999);
    ImGui::Checkbox("Send to inventory", &g_send_to_inventory);
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("Filters")) {
        ImGui::Checkbox("Show hidden", &g_show_hidden);
        ImGui::SameLine();
        ImGui::Checkbox("Show spoilers", &g_show_spoilers);
    }

    const std::string filter = lowercase(g_filter);
    ImGui::BeginChild("item_list", ImVec2(0.0f, 365.0f), true);
    if (ImGui::BeginTable("item_table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Price");
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(g_items.size()); ++i) {
            const ItemEntry& entry = g_items[i];
            if (!g_show_hidden && entry.hidden) {
                continue;
            }
            if (!g_show_spoilers && entry.spoiler) {
                continue;
            }
            if (!category_visible(entry)) {
                continue;
            }
            const std::string searchable = lowercase(entry.display + " " + entry.internal_name + " " + entry.category + " " + entry.class_name);
            if (!filter.empty() && searchable.find(filter) == std::string::npos) {
                continue;
            }

            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.tag.c_str());
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Selectable(entry.display.c_str(), g_selected == i, ImGuiSelectableFlags_SpanAllColumns)) {
                g_selected = i;
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.category.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", entry.price);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    if (g_selected >= 0 && g_selected < static_cast<int>(g_items.size())) {
        const ItemEntry& entry = g_items[g_selected];
        ImGui::Text("Selected: %s %s", entry.tag.c_str(), entry.display.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Spawn Selected")) {
            spawn_selected();
        }

        if (ImGui::CollapsingHeader("Selected Item Details")) {
            ImGui::TextDisabled("Internal: %s", entry.internal_name.c_str());
            ImGui::TextDisabled("Category: %s (%d)", entry.category.c_str(), entry.sdk_category);
            ImGui::TextDisabled("Class: %s", entry.class_name.c_str());
            ImGui::TextDisabled("Holdable: %s", entry.can_hold ? "yes" : "no");
            if (!entry.description.empty()) {
                ImGui::TextWrapped("%s", entry.description.c_str());
            }
        }
    } else {
        ImGui::TextDisabled("Select an item to spawn.");
    }

    if (!g_last_result.empty()) {
        ImGui::TextWrapped("%s", g_last_result.c_str());
    }
}

} // namespace votv::features
