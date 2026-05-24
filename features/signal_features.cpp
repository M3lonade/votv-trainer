#include "features/features.hpp"

#include "ue/sdk_context.hpp"

#include <imgui.h>

namespace votv::features {
namespace {

float g_download_multiplier = 1.0f;
float g_process_multiplier = 1.0f;
bool g_ignore_download_servers = false;
bool g_ignore_process_servers = false;

} // namespace

void render_signal_tab()
{
    const auto& ctx = ue::SdkContext::instance().snapshot();
    if (ctx.save_slot) {
        auto& download = ctx.save_slot->downloadPanelSignal;
        auto& processing = ctx.save_slot->processingPanelSignal;

        ImGui::TextUnformatted("Save-slot signal helpers");
        ImGui::Text("Download signal: %s", download.name_15_4DC53B564EDE34E0A8A16A92BD26B4AD.ToString().c_str());
        ImGui::Text("Downloaded %.2f / size %.2f", download.downloadedAtQuality_49_3546D7D84353CAD71C3E9B9CC0A62472, download.size_4_C0BC00CB4E2BC1C588F54A9817B305BC);
        if (ImGui::Button("Mark Download Signal Complete")) {
            download.downloadedAtQuality_49_3546D7D84353CAD71C3E9B9CC0A62472 = download.size_4_C0BC00CB4E2BC1C588F54A9817B305BC;
        }

        ImGui::Text("Processing signal: %s", processing.name_15_4DC53B564EDE34E0A8A16A92BD26B4AD.ToString().c_str());
        ImGui::Text("Decoded %.2f / size %.2f", processing.decoded_5_A9CAC26F480C342A406FFFB77DD0AB68, processing.size_4_C0BC00CB4E2BC1C588F54A9817B305BC);
        if (ImGui::Button("Mark Processing Signal Decoded")) {
            processing.decoded_5_A9CAC26F480C342A406FFFB77DD0AB68 = processing.size_4_C0BC00CB4E2BC1C588F54A9817B305BC;
        }

        ImGui::Separator();
    }

    if (!ctx.signal_panel) {
        ImGui::TextWrapped("Live signal panel controls are unavailable in this build. The save-slot helpers above are safer, but may require reopening the in-game panel or saving/loading for UI refresh.");
        return;
    }

    auto* panel = ctx.signal_panel;
    g_download_multiplier = panel->DL_downloadMultiplier;
    g_process_multiplier = panel->comp_processMultiplier;
    g_ignore_download_servers = panel->DL_ignoreServers;
    g_ignore_process_servers = panel->comp_ignoreServers;

    ImGui::Text("Download speed: %.3f", panel->DL_downloadSpeed);
    ImGui::Text("Download active: %s", panel->DL_downloading ? "yes" : "no");
    ImGui::Text("Process progress: %.3f", panel->comp_progress);
    ImGui::Text("Decode active: %s", panel->comp_isDecodeActive ? "yes" : "no");

    ImGui::Separator();
    ImGui::InputFloat("Download multiplier", &g_download_multiplier, 0.25f, 1.0f, "%.2f");
    ImGui::InputFloat("Process multiplier", &g_process_multiplier, 0.25f, 1.0f, "%.2f");
    ImGui::Checkbox("Ignore download servers", &g_ignore_download_servers);
    ImGui::Checkbox("Ignore process servers", &g_ignore_process_servers);

    if (ImGui::Button("Apply Signal Tweaks")) {
        panel->DL_downloadMultiplier = g_download_multiplier;
        panel->comp_processMultiplier = g_process_multiplier;
        panel->DL_ignoreServers = g_ignore_download_servers;
        panel->comp_ignoreServers = g_ignore_process_servers;
    }

    if (ImGui::Button("Set Fully Processed Signal Object")) {
        panel->setFullyProcessedSignalObject();
    }

    ImGui::TextWrapped("Signal controls are experimental because panel state is mirrored in drives, save data, power, and UI state.");
}

} // namespace votv::features
