#include "canvas_overlay.h"
#include "ui_scale.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <cmath>

void draw_canvas_overlays(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
                          const GLuint* onion_tex, ImVec2 origin, float W, float H) {
    // Checkerboard background — visible through transparent pixels
    {
        float cell = cs.checker_size * cs.zoom;
        ImU32 col1 = ImGui::ColorConvertFloat4ToU32(cs.checker_color1);
        ImU32 col2 = ImGui::ColorConvertFloat4ToU32(cs.checker_color2);
        dl->PushClipRect({origin.x, origin.y}, {origin.x + W, origin.y + H}, true);
        int row_count = static_cast<int>(H / cell) + 1;
        int col_count = static_cast<int>(W / cell) + 1;
        for (int row = 0; row < row_count; row++)
            for (int col = 0; col < col_count; col++) {
                float cx = origin.x + static_cast<float>(col) * cell;
                float cy = origin.y + static_cast<float>(row) * cell;
                ImU32 c  = (row + col) % 2 ? col2 : col1;
                dl->AddRectFilled({cx, cy}, {cx + cell, cy + cell}, c);
            }
        dl->PopClipRect();
    }

    // ImGui v1.92+ binds a linear sampler that overrides texture parameters — switch to nearest.
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if (pio.DrawCallback_SetSamplerNearest)
        dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);

    if (tools.onion_skin) {
        ImVec2 onion_max = {origin.x + W, origin.y + H};
        if (tools.onion_skin_mode != 2 && cs.active_frame > 0)
            dl->AddImage((ImTextureID)(uintptr_t)onion_tex[0], origin, onion_max,
                         {0, 0}, {1, 1}, IM_COL32(255, 128, 128, 128));
        if (tools.onion_skin_mode != 1 && cs.active_frame < (int)cs.frames.size() - 1)
            dl->AddImage((ImTextureID)(uintptr_t)onion_tex[1], origin, onion_max,
                         {0, 0}, {1, 1}, IM_COL32(128, 128, 255, 128));
    }
}

void draw_canvas_decorations(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
                              const SelectionState& sel,
                              ImVec2 origin, float W, float H,
                              const CanvasDragState& drag, ImVec2* hpos) {
    // Pixel grid overlay (zoom >= 4x, only when enabled)
    if (tools.show_grid && cs.zoom >= 4.0f) {
        ImU32 gcol = IM_COL32(80, 80, 80, 100);
        for (int x = 0; x <= cs.width(); x++) {
            float sx = origin.x + x * cs.zoom;
            dl->AddLine({sx, origin.y}, {sx, origin.y + H}, gcol);
        }
        for (int y = 0; y <= cs.height(); y++) {
            float sy = origin.y + y * cs.zoom;
            dl->AddLine({origin.x, sy}, {origin.x + W, sy}, gcol);
        }
    }

    // Symmetry axis guide lines
    if (tools.symmetry) {
        ImU32 axis_col = IM_COL32(255, 0, 255, 140);
        if (tools.symmetry_mode == 0 || tools.symmetry_mode == 2) {
            float ax = origin.x + cs.width() * 0.5f * cs.zoom;
            dl->AddLine({ax, origin.y}, {ax, origin.y + H}, axis_col, 1.0f);
        }
        if (tools.symmetry_mode == 1 || tools.symmetry_mode == 2) {
            float ay = origin.y + cs.height() * 0.5f * cs.zoom;
            dl->AddLine({origin.x, ay}, {origin.x + W, ay}, axis_col, 1.0f);
        }
    }

    // Marching ants selection overlay
    if (sel.active) {
        bool blink  = (int)(ImGui::GetTime() * 8) % 2;
        ImU32 col_a = blink ? IM_COL32(255,255,255,255) : IM_COL32(0,0,0,255);
        ImU32 col_b = blink ? IM_COL32(0,0,0,255) : IM_COL32(255,255,255,255);
        if (sel.mask.empty()) {
            ImVec2 sp1 = {origin.x + sel.x0 * cs.zoom,       origin.y + sel.y0 * cs.zoom};
            ImVec2 sp2 = {origin.x + (sel.x1 + 1) * cs.zoom, origin.y + (sel.y1 + 1) * cs.zoom};
            dl->AddRect(sp1, sp2, col_a, 0, 0, 1.5f);
            dl->AddRect({sp1.x+1,sp1.y+1},{sp2.x-1,sp2.y-1}, col_b, 0, 0, 1.0f);
        } else {
            int bw = sel.width();
            float z = cs.zoom;
            for (int ly = 0; ly < sel.height(); ly++) {
                for (int lx = 0; lx < sel.width(); lx++) {
                    if (!sel.mask[ly * bw + lx]) continue;
                    float sx = origin.x + (sel.x0 + lx) * z;
                    float sy = origin.y + (sel.y0 + ly) * z;
                    bool r = (lx + 1 >= sel.width())  || !sel.mask[ly * bw + (lx + 1)];
                    bool l = (lx - 1 < 0)             || !sel.mask[ly * bw + (lx - 1)];
                    bool d = (ly + 1 >= sel.height()) || !sel.mask[(ly + 1) * bw + lx];
                    bool u = (ly - 1 < 0)             || !sel.mask[(ly - 1) * bw + lx];
                    if (r) dl->AddLine({sx + z, sy},     {sx + z, sy + z}, col_a, 1.5f);
                    if (l) dl->AddLine({sx,     sy},     {sx,     sy + z}, col_a, 1.5f);
                    if (d) dl->AddLine({sx,     sy + z}, {sx + z, sy + z}, col_a, 1.5f);
                    if (u) dl->AddLine({sx,     sy},     {sx + z, sy},     col_a, 1.5f);
                }
            }
        }
    }

    // Floating selection pixel overlay — drawn on top of composite. Only the portion that
    // falls within the canvas is shown, so the user sees exactly what survives commit
    // (off-canvas pixels are dropped by commit_floating's bounds-checked writes).
    if (sel.floating) {
        for (int fy = 0; fy < sel.float_h; fy++) {
            for (int fx = 0; fx < sel.float_w; fx++) {
                if (!cs.active().in_bounds(sel.float_x + fx, sel.float_y + fy)) continue;
                uint32_t pv = sel.float_pixels[fy * sel.float_w + fx];
                if (((pv >> 24) & 0xFF) == 0) continue;
                float sx = origin.x + (sel.float_x + fx) * cs.zoom;
                float sy = origin.y + (sel.float_y + fy) * cs.zoom;
                dl->AddRectFilled({sx, sy}, {sx + cs.zoom, sy + cs.zoom}, pv);
            }
        }
    }

    // Selection resize handles — 8 handles around the bounding rect
    // Layout: TL=0 TM=1 TR=2 RM=3 BR=4 BM=5 BL=6 LM=7
    if (sel.active && tools.active_tool == tool::RectSelect) {
        float sx0 = origin.x + sel.x0 * cs.zoom;
        float sy0 = origin.y + sel.y0 * cs.zoom;
        float sx1 = origin.x + (sel.x1 + 1) * cs.zoom;
        float sy1 = origin.y + (sel.y1 + 1) * cs.zoom;
        float scx = (sx0 + sx1) * 0.5f, scy = (sy0 + sy1) * 0.5f;
        hpos[0]={sx0,sy0}; hpos[1]={scx,sy0}; hpos[2]={sx1,sy0}; hpos[3]={sx1,scy};
        hpos[4]={sx1,sy1}; hpos[5]={scx,sy1}; hpos[6]={sx0,sy1}; hpos[7]={sx0,scy};
        for (int i = 0; i < 8; i++) {
            bool is_active = drag.handle_dragging && drag.active_handle == i;
            ImU32 hcol = is_active ? IM_COL32(255,200,50,255) : IM_COL32(255,255,255,220);
            float hs = ui_scale::px(3.5f);
            dl->AddRectFilled({hpos[i].x-hs,hpos[i].y-hs},{hpos[i].x+hs,hpos[i].y+hs}, hcol);
            dl->AddRect({hpos[i].x-hs,hpos[i].y-hs},{hpos[i].x+hs,hpos[i].y+hs}, IM_COL32(0,0,0,200));
        }
    }
}
