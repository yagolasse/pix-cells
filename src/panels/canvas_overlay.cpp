#include "canvas_overlay.h"
#include "ui_scale.h"
#include "imgui.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <cmath>

// Cached 2x2 checker texture shared across all documents (application-lifetime, tiny resource)
static GLuint s_checker_tex = 0;
static ImU32  s_ck1 = 0, s_ck2 = 0;

void draw_canvas_overlays(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
                          const GLuint* onion_tex, ImVec2 origin, float W, float H) {
    // Checkerboard background — visible through transparent pixels
    {
        float cell = cs.checker_size * cs.zoom;
        ImU32 col1 = ImGui::ColorConvertFloat4ToU32(cs.checker_color1);
        ImU32 col2 = ImGui::ColorConvertFloat4ToU32(cs.checker_color2);
        // Rebuild 2x2 checker texture only when colors change
        if (col1 != s_ck1 || col2 != s_ck2) {
            if (s_checker_tex != 0)
                glDeleteTextures(1, &s_checker_tex);
            glGenTextures(1, &s_checker_tex);
            glBindTexture(GL_TEXTURE_2D, s_checker_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            // 2x2 RGBA8 checker: [col1, col2 / col2, col1]; ImU32 = R bits 0-7 = GL_RGBA/GL_UNSIGNED_BYTE
            uint32_t pixels[4] = {col1, col2, col2, col1};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            s_ck1 = col1;
            s_ck2 = col2;
        }
        // ImGui v1.92+ binds sampler objects with GL_CLAMP_TO_EDGE, which overrides the
        // texture's own GL_REPEAT wrap state and breaks UV tiling. Unbind any sampler object
        // before this draw call so the texture's GL_NEAREST + GL_REPEAT params take effect.
        // glBindSampler is GL 3.3 core but not declared by SDL_opengl.h — load it once.
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            using BindSamplerFn = void(*)(GLuint, GLuint);
            static auto fn = reinterpret_cast<BindSamplerFn>(
                SDL_GL_GetProcAddress("glBindSampler"));
            if (fn) fn(0, 0);
        }, nullptr);
        dl->PushClipRect({origin.x, origin.y}, {origin.x + W, origin.y + H}, true);
        // One draw call: UV spans = screen area / one checker period (2 cells)
        float pu = W / (2.f * cell);
        float pv = H / (2.f * cell);
        dl->AddImage((ImTextureID)(intptr_t)s_checker_tex,
                     {origin.x, origin.y}, {origin.x + W, origin.y + H},
                     {0.f, 0.f}, {pu, pv});
        dl->PopClipRect();
    }

    // Restore ImGui's nearest sampler for subsequent pixel-art texture draws.
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
                              DocRenderState& drs, ImVec2* hpos) {
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
            float z = cs.zoom;
            if (drs.ant_sel_rev != sel.sel_revision) {
                drs.ant_edges.clear();
                int bw = sel.width();
                for (int ly = 0; ly < sel.height(); ly++) {
                    for (int lx = 0; lx < sel.width(); lx++) {
                        if (!sel.mask[ly * bw + lx]) continue;
                        int ax = sel.x0 + lx, ay = sel.y0 + ly;
                        bool r = (lx+1 >= bw)           || !sel.mask[ly*bw+(lx+1)];
                        bool l = (lx-1 < 0)             || !sel.mask[ly*bw+(lx-1)];
                        bool d = (ly+1 >= sel.height()) || !sel.mask[(ly+1)*bw+lx];
                        bool u = (ly-1 < 0)             || !sel.mask[(ly-1)*bw+lx];
                        if (r) drs.ant_edges.push_back({ax+1, ay,   ax+1, ay+1});
                        if (l) drs.ant_edges.push_back({ax,   ay,   ax,   ay+1});
                        if (d) drs.ant_edges.push_back({ax,   ay+1, ax+1, ay+1});
                        if (u) drs.ant_edges.push_back({ax,   ay,   ax+1, ay  });
                    }
                }
                drs.ant_sel_rev = sel.sel_revision;
            }
            for (const auto& seg : drs.ant_edges) {
                float sx0 = origin.x + static_cast<float>(seg.x0) * z;
                float sy0 = origin.y + static_cast<float>(seg.y0) * z;
                float sx1 = origin.x + static_cast<float>(seg.x1) * z;
                float sy1 = origin.y + static_cast<float>(seg.y1) * z;
                dl->AddLine({sx0, sy0}, {sx1, sy1}, col_a, 1.5f);
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
            bool is_active = drs.drag.handle_dragging && drs.drag.active_handle == i;
            ImU32 hcol = is_active ? IM_COL32(255,200,50,255) : IM_COL32(255,255,255,220);
            float hs = ui_scale::px(3.5f);
            dl->AddRectFilled({hpos[i].x-hs,hpos[i].y-hs},{hpos[i].x+hs,hpos[i].y+hs}, hcol);
            dl->AddRect({hpos[i].x-hs,hpos[i].y-hs},{hpos[i].x+hs,hpos[i].y+hs}, IM_COL32(0,0,0,200));
        }
    }
}
