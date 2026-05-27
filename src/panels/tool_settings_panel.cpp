#include "tool_settings_panel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "icon_manager.h"
#include "ui_scale.h"
#include <algorithm>

static bool active_icon_btn(const char* id, ImTextureID tex, float sz, bool is_active) {
    if (is_active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    bool clicked = ImGui::ImageButton(id, tex, ImVec2(sz, sz));
    if (is_active)
        ImGui::PopStyleColor();
    return clicked;
}

void panels::DrawToolSettings(ToolsState& state) {
    ImGui::Begin("Tool Settings");

    int t = state.active_tool;
    bool show_symmetry_mode = state.symmetry;
    bool show_onion_mode    = state.onion_skin;
    bool show_brush_size    = (t == tool::Brush || t == tool::Eraser || t == tool::Line);
    bool show_brush_shape   = (t == tool::Brush || t == tool::Eraser);

    if (!show_symmetry_mode && !show_onion_mode && !show_brush_size && !show_brush_shape) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("No settings");
        ImGui::End();
        return;
    }

    bool first = true;

    if (show_symmetry_mode) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Mirror:");
        ImGui::SameLine();
        const char* sym_modes[] = {"Horizontal", "Vertical", "Both"};
        ImGui::SetNextItemWidth(ui_scale::px(90.0f));
        ImGui::Combo("##sym_mode", &state.symmetry_mode, sym_modes, 3);
        first = false;
    }

    if (show_onion_mode) {
        if (!first) {
            ImGui::SameLine(0, 8.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0, 8.0f);
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Onion:");
        ImGui::SameLine();
        const char* modes[] = {"Both", "Previous", "Next"};
        ImGui::SetNextItemWidth(ui_scale::px(80.0f));
        ImGui::Combo("##onion_mode", &state.onion_skin_mode, modes, 3);
        first = false;
    }

    if (show_brush_size) {
        if (!first) {
            ImGui::SameLine(0, 8.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0, 8.0f);
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Size");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ui_scale::px(50.0f));
        ImGui::DragInt("##bsz", &state.brush_size, 0.2f, 1, 32, "%d px");
        state.brush_size = std::clamp(state.brush_size, 1, 32);
        first = false;
    }

    if (show_brush_shape) {
        if (!first) {
            ImGui::SameLine(0, 8.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0, 8.0f);
        }
        const float FP = ui_scale::px(4.0f);
        const float SZ = ui_scale::px(22.0f * 0.75f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(FP, FP));
        if (active_icon_btn("##bsh0", icon_manager::get("square_filled"), SZ, !state.circle_brush))
            state.circle_brush = false;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Square brush");
        ImGui::SameLine();
        if (active_icon_btn("##bsh1", icon_manager::get("circle_filled"), SZ, state.circle_brush))
            state.circle_brush = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Circle brush");
        ImGui::PopStyleVar();
    }

    ImGui::End();
}
