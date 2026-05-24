#include "render/theme.hpp"

#include <imgui.h>

#include <cmath>

namespace votv::render::theme {
namespace {

ImU32 color(float r, float g, float b, float a)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

float wave(float value)
{
    return (std::sin(value) + 1.0f) * 0.5f;
}

} // namespace

void apply_space_theme()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 10.0f;
    style.WindowRounding = 12.0f;
    style.ChildRounding = 9.0f;
    style.FrameRounding = 7.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.82f, 0.92f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.54f, 0.63f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.018f, 0.025f, 0.045f, 0.96f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.030f, 0.045f, 0.075f, 0.84f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.025f, 0.035f, 0.060f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.16f, 0.38f, 0.52f, 0.55f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.045f, 0.075f, 0.110f, 0.94f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.080f, 0.150f, 0.210f, 0.96f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.100f, 0.230f, 0.310f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.018f, 0.025f, 0.045f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.030f, 0.080f, 0.125f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.018f, 0.025f, 0.045f, 0.90f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.025f, 0.045f, 0.075f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.020f, 0.030f, 0.050f, 0.82f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.105f, 0.280f, 0.370f, 0.95f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.145f, 0.420f, 0.540f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.230f, 0.650f, 0.760f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.25f, 0.92f, 0.83f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.16f, 0.62f, 0.72f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.95f, 0.72f, 0.28f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.060f, 0.170f, 0.245f, 0.94f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.095f, 0.310f, 0.420f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.170f, 0.520f, 0.620f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.055f, 0.180f, 0.250f, 0.88f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.090f, 0.300f, 0.410f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.140f, 0.450f, 0.540f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.55f, 0.65f, 0.45f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.25f, 0.75f, 0.88f, 0.70f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.95f, 0.74f, 0.28f, 0.90f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.55f, 0.65f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.25f, 0.75f, 0.88f, 0.55f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.95f, 0.74f, 0.28f, 0.80f);
    colors[ImGuiCol_Tab] = ImVec4(0.030f, 0.075f, 0.120f, 0.95f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.090f, 0.310f, 0.430f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.060f, 0.210f, 0.300f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.020f, 0.045f, 0.075f, 0.95f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.040f, 0.140f, 0.210f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.040f, 0.120f, 0.170f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.18f, 0.45f, 0.55f, 0.70f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.10f, 0.25f, 0.33f, 0.45f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.08f, 0.18f, 0.24f, 0.16f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.15f, 0.55f, 0.70f, 0.35f);
}

void draw_space_window_background()
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 min = pos;
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    const float time = static_cast<float>(ImGui::GetTime());

    draw->PushClipRect(min, max, true);

    draw->AddRectFilledMultiColor(
        min,
        max,
        color(0.015f, 0.020f, 0.040f, 0.72f),
        color(0.018f, 0.055f, 0.085f, 0.72f),
        color(0.030f, 0.025f, 0.070f, 0.72f),
        color(0.012f, 0.015f, 0.030f, 0.72f));

    for (int i = 0; i < 84; ++i) {
        const float seed = static_cast<float>(i);
        const float x = pos.x + std::fmod(seed * 73.13f + time * (4.0f + std::fmod(seed, 5.0f)), size.x);
        const float y = pos.y + std::fmod(seed * 41.71f + time * (8.0f + std::fmod(seed, 7.0f)), size.y);
        const float brightness = 0.18f + 0.42f * wave(time * 1.7f + seed);
        const float radius = (i % 11 == 0) ? 1.55f : 1.0f;
        draw->AddCircleFilled(ImVec2(x, y), radius, color(0.55f, 0.88f, 1.0f, brightness));
    }

    for (int i = 0; i < 3; ++i) {
        const float seed = static_cast<float>(i);
        const float phase = std::fmod(time * (0.18f + seed * 0.025f) + seed * 0.33f, 1.0f);
        const float x = pos.x + phase * (size.x + 240.0f) - 120.0f;
        const float y = pos.y + 70.0f + seed * 135.0f + 22.0f * std::sin(time * 0.7f + seed);
        draw->AddLine(ImVec2(x, y), ImVec2(x - 75.0f, y + 34.0f), color(0.45f, 0.90f, 1.0f, 0.22f), 1.25f);
    }

    const float baseline = max.y - 58.0f;
    for (int i = 0; i < 48; ++i) {
        const float x0 = pos.x + 20.0f + i * ((size.x - 40.0f) / 48.0f);
        const float x1 = pos.x + 20.0f + (i + 1) * ((size.x - 40.0f) / 48.0f);
        const float y0 = baseline + std::sin(time * 1.6f + i * 0.45f) * 8.0f;
        const float y1 = baseline + std::sin(time * 1.6f + (i + 1) * 0.45f) * 8.0f;
        draw->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color(0.22f, 0.82f, 0.78f, 0.12f), 1.0f);
    }

    for (float y = min.y + 38.0f; y < max.y; y += 18.0f) {
        draw->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), color(0.35f, 0.75f, 0.90f, 0.025f), 1.0f);
    }

    draw->AddRect(min, max, color(0.18f, 0.75f, 0.88f, 0.38f), ImGui::GetStyle().WindowRounding, 0, 1.25f);
    draw->PopClipRect();
}

} // namespace votv::render::theme
