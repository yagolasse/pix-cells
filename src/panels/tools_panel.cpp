#include "tools_panel.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <algorithm>

static ImFont* s_icon_font = nullptr;
void panels::SetIconFont(ImFont* font) {
    s_icon_font = font;
}

void panels::DrawTools(ToolsState& state) {
    static bool s_symmetry = false;
    static bool s_onion    = false;

    const float BTN = 32.0f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(BTN + 16.0f, 0), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Tools");
    const float avail = ImGui::GetContentRegionAvail().x;
    const float pad   = std::max(0.0f, (avail - BTN) * 0.5f);

    struct {
        const char* label;
        const char* tip;
    } tool_defs[] = {
        {ICON_FA_PAINTBRUSH "##t0",    "Brush  (B)"},
        {ICON_FA_ERASER "##t1",        "Eraser  (E)"},
        {ICON_FA_FILL_DRIP "##t2",     "Fill  (F)"},
        {ICON_FA_PEN "##t3",           "Line  (L)"},
        {ICON_FA_VECTOR_SQUARE "##t4", "Rect  (R)"},
        {ICON_FA_SQUARE_FULL "##t5",   "Filled Rect"},
        {ICON_FA_RING "##t6",          "Circle  (C)"},
        {ICON_FA_CIRCLE "##t7",        "Filled Circle"},
        {ICON_FA_HAND "##t8",          "Move  (M)"},
        {ICON_FA_OBJECT_GROUP "##t9",  "Rect Select  (S)"},
    };

    if (s_icon_font)
        ImGui::PushFont(s_icon_font);

    for (int i = 0; i < 10; i++) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        bool active = (state.active_tool == i);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(tool_defs[i].label, ImVec2(BTN, BTN)))
            state.active_tool = i;
        if (active)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            if (s_icon_font) ImGui::PopFont();
            ImGui::SetTooltip("%s", tool_defs[i].tip);
            if (s_icon_font) ImGui::PushFont(s_icon_font);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    struct {
        bool* val;
        const char* label;
        const char* tip;
    } view_defs[] = {
        {&s_symmetry, ICON_FA_LEFT_RIGHT "##sym", "Symmetry"},
        {&state.show_grid, ICON_FA_BORDER_ALL "##grid", "Grid"},
        {&s_onion, ICON_FA_CLONE "##onion", "Onion skin"},
    };

    for (auto& v : view_defs) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        if (*v.val)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(v.label, ImVec2(BTN, BTN)))
            *v.val = !*v.val;
        if (*v.val)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            if (s_icon_font) ImGui::PopFont();
            ImGui::SetTooltip("%s", v.tip);
            if (s_icon_font) ImGui::PushFont(s_icon_font);
        }
    }

    if (s_icon_font)
        ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    int t = state.active_tool;
    bool show_brush_size  = (t == 0 || t == 1 || t == 3);
    bool show_brush_shape = (t == 0 || t == 1);

    if (show_brush_size) {
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
    }

    if (show_brush_shape) {
        ImGui::Spacing();
        if (s_icon_font)
            ImGui::PushFont(s_icon_font);
        const float SZ = BTN * 0.75f;
        bool sq = !state.circle_brush;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - SZ) * 0.5f));
        if (sq)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_SQUARE_FULL "##bsh0", ImVec2(SZ, SZ)))
            state.circle_brush = false;
        if (sq)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            if (s_icon_font) ImGui::PopFont();
            ImGui::SetTooltip("Square brush");
            if (s_icon_font) ImGui::PushFont(s_icon_font);
        }
        bool circ = state.circle_brush;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - SZ) * 0.5f));
        if (circ)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_CIRCLE "##bsh1", ImVec2(SZ, SZ)))
            state.circle_brush = true;
        if (circ)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            if (s_icon_font) ImGui::PopFont();
            ImGui::SetTooltip("Circle brush");
            if (s_icon_font) ImGui::PushFont(s_icon_font);
        }
        if (s_icon_font)
            ImGui::PopFont();
    }

    ImGui::End();
}
