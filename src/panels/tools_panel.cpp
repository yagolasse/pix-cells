#include "tools_panel.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>

// SVG-like icons drawn as ImDrawList primitives. Base grid is 16x16; `sz` scales it.
static void draw_tool_icon(ImDrawList* dl, ImVec2 o, float sz, int tool, ImU32 col) {
    const float s  = sz / 16.0f;
    const float sw = 1.5f * s;
    auto p = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    switch (tool) {
        case 0: // Brush: diagonal stroke + dot tip
            dl->AddLine(p(3,13), p(10,6), col, sw);
            dl->AddLine(p(10,6), p(13,3), col, sw);
            dl->AddCircleFilled(p(4,12), 1.2f * s, col);
            break;
        case 1: // Eraser: rect with center divider
            dl->AddRect(p(3,6), p(13,12), col, 1.5f * s, 0, sw);
            dl->AddLine(p(8,6), p(8,12), col, sw);
            break;
        case 2: // Bucket: filled square
            dl->AddRectFilled(p(3,3), p(13,13), col);
            break;
        case 3: // Line: diagonal
            dl->AddLine(p(3,13), p(13,3), col, sw);
            break;
        case 4: // Rect: outlined
            dl->AddRect(p(3,3), p(13,13), col, 0.0f, 0, sw);
            break;
        case 5: // Circle: outlined
            dl->AddCircle(p(8,8), 5.0f * s, col, 32, sw);
            break;
    }
}

static void draw_view_icon(ImDrawList* dl, ImVec2 o, float sz, int icon, ImU32 col) {
    const float s  = sz / 16.0f;
    const float sw = 1.5f * s;
    auto p = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    switch (icon) {
        case 0: // Symmetry: dashed center line + two side rects
            for (int i = 0; i < 6; i++) {
                float y0 = (2.0f + i * 2.0f) * s;
                dl->AddLine(ImVec2(o.x + 8.0f * s, o.y + y0),
                            ImVec2(o.x + 8.0f * s, o.y + y0 + s), col, sw);
            }
            dl->AddRect(p(3,5),  p(7,11),  col, 0.0f, 0, sw);
            dl->AddRect(p(9,5),  p(13,11), col, 0.0f, 0, sw);
            break;
        case 1: // Grid: rect + cross
            dl->AddRect(p(3,3),  p(13,13), col, 0.0f, 0, sw);
            dl->AddLine(p(3,8),  p(13,8),  col, sw);
            dl->AddLine(p(8,3),  p(8,13),  col, sw);
            break;
        case 2: // Onion skin: two overlapping circles
            dl->AddCircle(p(6,8),  3.5f * s, col, 32, sw);
            dl->AddCircle(p(10,8), 3.5f * s, col, 32, sw);
            break;
    }
}

void panels::DrawTools(ToolsState& state) {
    static bool s_symmetry = false;
    static bool s_grid     = false;
    static bool s_onion    = false;

    ImGui::Begin("Tools");
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float BTN   = 32.0f;
    const float ICN   = 16.0f;
    const float pad   = std::max(0.0f, (avail - BTN) * 0.5f);

    // --- Tool buttons ---
    struct { const char* id; const char* tip; } tool_defs[] = {
        { "##t0", "Brush  (B)"  },
        { "##t1", "Eraser  (E)" },
        { "##t2", "Bucket  (G)" },
        { "##t3", "Line  (L)"   },
        { "##t4", "Rect  (U)"   },
        { "##t5", "Circle  (C)" },
    };

    for (int i = 0; i < 6; i++) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        ImVec2 pos     = ImGui::GetCursorScreenPos();
        bool   clicked = ImGui::InvisibleButton(tool_defs[i].id, ImVec2(BTN, BTN));
        bool   hov     = ImGui::IsItemHovered();
        bool   active  = (state.active_tool == i);

        ImU32 bg = active ? ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive))
                  : hov   ? ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered))
                          : ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        dl->AddRectFilled(pos, ImVec2(pos.x + BTN, pos.y + BTN), bg, 2.0f);

        ImU32 icol = (active || hov) ? IM_COL32(255, 255, 255, 255)
                                     : ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
        draw_tool_icon(dl, ImVec2(pos.x + (BTN - ICN) * 0.5f, pos.y + (BTN - ICN) * 0.5f), ICN, i, icol);

        if (clicked) state.active_tool = i;
        if (hov)     ImGui::SetTooltip("%s", tool_defs[i].tip);
    }

    // --- Separator ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- View toggles ---
    struct { bool* val; const char* id; const char* tip; int icon; } view_defs[] = {
        { &s_symmetry, "##sym",   "Symmetry",   0 },
        { &s_grid,     "##grid",  "Grid",       1 },
        { &s_onion,    "##onion", "Onion skin", 2 },
    };

    for (auto& v : view_defs) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        ImVec2 pos     = ImGui::GetCursorScreenPos();
        bool   clicked = ImGui::InvisibleButton(v.id, ImVec2(BTN, BTN));
        bool   hov     = ImGui::IsItemHovered();
        bool   active  = *v.val;

        ImU32 bg = active ? ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive))
                  : hov   ? ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered))
                          : ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        dl->AddRectFilled(pos, ImVec2(pos.x + BTN, pos.y + BTN), bg, 2.0f);

        ImU32 icol = (active || hov) ? IM_COL32(255, 255, 255, 255)
                                     : ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
        draw_view_icon(dl, ImVec2(pos.x + (BTN - ICN) * 0.5f, pos.y + (BTN - ICN) * 0.5f), ICN, v.icon, icol);

        if (clicked) *v.val = !*v.val;
        if (hov)     ImGui::SetTooltip("%s", v.tip);
    }

    // --- Separator ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Brush size ---
    // Dim label centered
    {
        char lbl[12];
        snprintf(lbl, sizeof(lbl), "%d px", state.brush_size);
        float lw = ImGui::CalcTextSize(lbl).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - lw) * 0.5f);
        ImGui::TextDisabled("%s", lbl);
    }
    // DragInt centered at 36px wide
    {
        const float W = 36.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - W) * 0.5f));
        ImGui::SetNextItemWidth(W);
        ImGui::DragInt("##bsz", &state.brush_size, 0.2f, 1, 32, "%d");
        state.brush_size = std::clamp(state.brush_size, 1, 32);
    }

    ImGui::End();
}
