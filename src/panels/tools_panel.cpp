#include "tools_panel.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <algorithm>

static ImFont* s_icon_font = nullptr;
void panels::SetIconFont(ImFont* font) { s_icon_font = font; }

void panels::DrawTools(ToolsState& state) {
    static bool s_symmetry = false;
    static bool s_onion    = false;

    ImGui::Begin("Tools");
    const float avail = ImGui::GetContentRegionAvail().x;
    const float BTN   = 32.0f;
    const float pad   = std::max(0.0f, (avail - BTN) * 0.5f);

    struct { const char* label; const char* tip; } tool_defs[] = {
        { ICON_FA_PAINTBRUSH           "##t0", "Brush  (B)"  },
        { ICON_FA_ERASER               "##t1", "Eraser  (E)" },
        { ICON_FA_FILL_DRIP            "##t2", "Fill  (F)"   },
        { ICON_FA_PEN                  "##t3", "Line  (L)"   },
        { ICON_FA_SQUARE               "##t4", "Rect  (R)"   },
        { ICON_FA_CIRCLE               "##t5", "Circle  (C)" },
        { ICON_FA_HAND                 "##t6", "Move  (M)"   },
    };

    if (s_icon_font) ImGui::PushFont(s_icon_font);

    for (int i = 0; i < 7; i++) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        bool active = (state.active_tool == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(tool_defs[i].label, ImVec2(BTN, BTN))) state.active_tool = i;
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tool_defs[i].tip);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    struct { bool* val; const char* label; const char* tip; } view_defs[] = {
        { &s_symmetry,        ICON_FA_LEFT_RIGHT "##sym",   "Symmetry"   },
        { &state.show_grid,   ICON_FA_BORDER_ALL "##grid",  "Grid"       },
        { &s_onion,           ICON_FA_CLONE      "##onion", "Onion skin" },
    };

    for (auto& v : view_defs) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        if (*v.val) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(v.label, ImVec2(BTN, BTN))) *v.val = !*v.val;
        if (*v.val) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", v.tip);
    }

    if (s_icon_font) ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    {
        char lbl[12];
        snprintf(lbl, sizeof(lbl), "%d px", state.brush_size);
        float lw = ImGui::CalcTextSize(lbl).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - lw) * 0.5f);
        ImGui::TextDisabled("%s", lbl);
    }
    {
        const float W = 36.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - W) * 0.5f));
        ImGui::SetNextItemWidth(W);
        ImGui::DragInt("##bsz", &state.brush_size, 0.2f, 1, 32, "%d");
        state.brush_size = std::clamp(state.brush_size, 1, 32);
    }

    ImGui::End();
}
