#include "tools_panel.h"
#include "imgui.h"
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

void panels::DrawTools(ToolsState& state) {
    const float ICON = ui_scale::px(22.0f);
    const float FP   = ui_scale::px(4.0f);
    const float BTN  = ICON + FP * 2; // total button footprint
    ImGui::SetNextWindowSizeConstraints(ImVec2(BTN + 16.0f, 0), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Tools");
    const float avail = ImGui::GetContentRegionAvail().x;
    const float pad   = std::max(0.0f, (avail - BTN) * 0.5f);

    struct {
        const char* icon;
        const char* id;
        const char* tip;
    } tool_defs[] = {
        {"brush",         "##t0",  "Brush  (B)"},
        {"eraser",        "##t1",  "Eraser  (E)"},
        {"fill",          "##t2",  "Fill  (F)"},
        {"line",          "##t3",  "Line  (L)"},
        {"square",        "##t4",  "Rect  (R)"},
        {"square_filled", "##t5",  "Filled Rect  (R)"},
        {"circle",        "##t6",  "Circle  (U)"},
        {"circle_filled", "##t7",  "Filled Circle  (U)"},
        {"pan_tool",      "##t8",  "Move  (M)"},
        {"select",        "##t9",  "Rect Select  (S)"},
        {"eyedropper",    "##t10", "Color Picker  (I)"},
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(FP, FP));

    for (int i = 0; i < (int)std::size(tool_defs); i++) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        if (active_icon_btn(tool_defs[i].id, icon_manager::get(tool_defs[i].icon), ICON, state.active_tool == i))
            state.active_tool = i;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tool_defs[i].tip);
    }

    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    struct {
        bool* val;
        const char* icon_on;
        const char* icon_off;
        const char* tip;
    } view_defs[] = {
        {&state.symmetry,   "symmetry",  "symmetry",  "Symmetry"},
        {&state.show_grid,  "grid",      "grid_off",  "Grid"},
        {&state.onion_skin, "onion_skin","onion_skin", "Onion skin"},
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(FP, FP));

    for (auto& v : view_defs) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        bool active = *v.val;
        if (active_icon_btn(v.tip, icon_manager::get(active ? v.icon_on : v.icon_off), ICON, active))
            *v.val = !*v.val;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", v.tip);
    }

    ImGui::PopStyleVar();

    ImGui::End();
}
