#include "canvas_panel.h"
#include "raster.h"
#include "log.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Handle move table: {moves_x0, moves_y0, moves_x1, moves_y1}
// Handles: TL=0 TM=1 TR=2 RM=3 BR=4 BM=5 BL=6 LM=7
static constexpr bool k_hmoves[8][4] = {
    {true,true,false,false}, {false,true,false,false}, {false,true,true,false}, {false,false,true,false},
    {false,false,true,true}, {false,false,false,true}, {true,false,false,true}, {true,false,false,false}
};

struct CanvasDragState {
    bool shape_dragging  = false;
    int  shape_sx = -1, shape_sy = -1;
    int  float_drag_ox = 0, float_drag_oy = 0;
    int  active_handle    = -1;
    bool handle_dragging  = false;
    int  handle_start_x0 = 0, handle_start_y0 = 0, handle_start_x1 = 0, handle_start_y1 = 0;
};

static void draw_canvas_overlays(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
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

static void draw_canvas_decorations(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
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
        ImVec2 sp1 = {origin.x + sel.x0 * cs.zoom,          origin.y + sel.y0 * cs.zoom};
        ImVec2 sp2 = {origin.x + (sel.x1 + 1) * cs.zoom,    origin.y + (sel.y1 + 1) * cs.zoom};
        bool blink  = (int)(ImGui::GetTime() * 8) % 2;
        dl->AddRect(sp1, sp2,
            blink ? IM_COL32(255,255,255,255) : IM_COL32(0,0,0,255), 0, 0, 1.5f);
        dl->AddRect({sp1.x+1,sp1.y+1},{sp2.x-1,sp2.y-1},
            blink ? IM_COL32(0,0,0,255) : IM_COL32(255,255,255,255), 0, 0, 1.0f);
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
            dl->AddRectFilled({hpos[i].x-3.5f,hpos[i].y-3.5f},{hpos[i].x+3.5f,hpos[i].y+3.5f}, hcol);
            dl->AddRect({hpos[i].x-3.5f,hpos[i].y-3.5f},{hpos[i].x+3.5f,hpos[i].y+3.5f}, IM_COL32(0,0,0,200));
        }
    }
}

void panels::DrawCanvas(CanvasState& cs, ToolsState& tools, PaletteState& palette, SelectionState& sel) {
    static GLuint texture = 0;
    static GLuint onion_tex[2] = {0, 0};
    static std::vector<uint32_t> onion_buf;
    static int tex_w = 0, tex_h = 0;
    static ImVec2 last_px       = {-1.0f, -1.0f};
    static bool was_painting    = false;
    static CanvasDragState drag;

    // (Re)create texture on first call or if canvas dimensions changed
    if (texture == 0 || tex_w != cs.width() || tex_h != cs.height()) {
        if (texture != 0)
            glDeleteTextures(1, &texture);
        if (onion_tex[0] != 0)
            glDeleteTextures(2, onion_tex);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.width(), cs.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     cs.composite.data());
        glGenTextures(2, onion_tex);
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, onion_tex[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.width(), cs.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        tex_w    = cs.width();
        tex_h    = cs.height();
        cs.dirty = false;
    } else if (cs.dirty) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                        cs.composite.data());
        cs.dirty = false;
    }

    if (tools.onion_skin) {
        bool show_prev = tools.onion_skin_mode != 2 && cs.active_frame > 0;
        bool show_next = tools.onion_skin_mode != 1 && cs.active_frame < (int)cs.frames.size() - 1;
        if (show_prev) {
            cs.composite_frame(cs.active_frame - 1, onion_buf);
            glBindTexture(GL_TEXTURE_2D, onion_tex[0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE, onion_buf.data());
        }
        if (show_next) {
            cs.composite_frame(cs.active_frame + 1, onion_buf);
            glBindTexture(GL_TEXTURE_2D, onion_tex[1]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE, onion_buf.data());
        }
    }

    ImGui::Begin("Canvas");

    // Commit floating selection automatically when tool changes away from rect select
    if (sel.floating && tools.active_tool != tool::RectSelect) {
        commit_floating(cs, sel);
        drag.handle_dragging = false;
        drag.active_handle   = -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool any_popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

    // Center canvas once the docking layout has settled.
    // On first launch (no imgui.ini), the panel resizes over several frames as docking applies.
    // We compare avail against the previous frame's value; only center when both are stable.
    if (cs.needs_center) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 0 && avail.y > 0) {
            static ImVec2 prev_avail = {0, 0};
            if (avail.x == prev_avail.x && avail.y == prev_avail.y) {
                float W         = cs.width() * cs.zoom;
                float H         = cs.height() * cs.zoom;
                cs.pan          = {(avail.x - W) * 0.5f, (avail.y - H) * 0.5f};
                cs.needs_center = false;
            }
            prev_avail = avail;
        }
    }

    // Pan with middle-mouse drag
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        cs.pan.x += io.MouseDelta.x;
        cs.pan.y += io.MouseDelta.y;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 base    = ImGui::GetCursorScreenPos();
    ImVec2 origin  = {std::round(base.x + cs.pan.x), std::round(base.y + cs.pan.y)};
    float W        = cs.width() * cs.zoom;
    float H        = cs.height() * cs.zoom;

    ImVec2 hpos[8] = {};
    draw_canvas_overlays(dl, cs, tools, onion_tex, origin, W, H);

    ImGui::SetCursorScreenPos(origin);
    ImGui::Image((ImTextureID)(uintptr_t)texture, {W, H});

    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if (pio.DrawCallback_SetSamplerLinear)
        dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);

    draw_canvas_decorations(dl, cs, tools, sel, origin, W, H, drag, hpos);

    // Tool input — compute canvas coords early so shape commit can use them outside hovered block
    ImVec2 cur_origin = {std::round(base.x + cs.pan.x), std::round(base.y + cs.pan.y)};
    int px            = (int)((io.MousePos.x - cur_origin.x) / cs.zoom);
    int py            = (int)((io.MousePos.y - cur_origin.y) / cs.zoom);

    // Shape preview overlay — pixel-exact, mirrors the committed drawing algorithms
    if (drag.shape_dragging && !sel.floating) {
        ImU32 prev_col = (ImGui::ColorConvertFloat4ToU32(palette.primary_color) & 0x00FFFFFFu) | 0xCC000000u;
        float z = cs.zoom;

        auto draw_px_base = [&](int cx, int cy) {
            if (cx < 0 || cx >= cs.width() || cy < 0 || cy >= cs.height()) return;
            float sx = cur_origin.x + static_cast<float>(cx) * z;
            float sy = cur_origin.y + static_cast<float>(cy) * z;
            dl->AddRectFilled({sx, sy}, {sx + z, sy + z}, prev_col);
        };
        auto draw_px = [&](int cx, int cy) {
            int W2 = cs.width(), H2 = cs.height();
            draw_px_base(cx, cy);
            if (tools.symmetry) {
                if (tools.symmetry_mode == 0 || tools.symmetry_mode == 2) draw_px_base(W2-1-cx, cy);
                if (tools.symmetry_mode == 1 || tools.symmetry_mode == 2) draw_px_base(cx, H2-1-cy);
                if (tools.symmetry_mode == 2) draw_px_base(W2-1-cx, H2-1-cy);
            }
        };

        auto stamp = [&](int cx, int cy) {
            int half = tools.brush_size / 2;
            for (int dy = -half; dy <= half; dy++)
                for (int dx = -half; dx <= half; dx++) {
                    if (tools.circle_brush && dx * dx + dy * dy > half * half) continue;
                    draw_px(cx + dx, cy + dy);
                }
        };

        int t = tools.active_tool;
        // Shift constrains rect/circle to a square bounding box
        int epx = px, epy = py;
        if (io.KeyShift && tool::is_shape(t)) {
            int ddx = px - drag.shape_sx, ddy = py - drag.shape_sy;
            int ax = std::abs(ddx), ay = std::abs(ddy);
            // 1-pixel hysteresis: when deltas are ≤1 apart (pen near 45°), round up
            // instead of down so 1-pixel tablet jitter doesn't oscillate between N and N+1.
            int d = (std::abs(ax - ay) <= 1) ? std::max(ax, ay) : std::min(ax, ay);
            epx   = drag.shape_sx + (ddx < 0 ? -d : d);
            epy   = drag.shape_sy + (ddy < 0 ? -d : d);
        }
        if (t == tool::Line) {  // Line — Bresenham + brush stamp
            int x0 = drag.shape_sx, y0 = drag.shape_sy, x1 = px, y1 = py;
            int adx = std::abs(x1 - x0), stepx = x0 < x1 ? 1 : -1;
            int ady = -std::abs(y1 - y0), stepy = y0 < y1 ? 1 : -1;
            int err = adx + ady;
            while (true) {
                stamp(x0, y0);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= ady) { err += ady; x0 += stepx; }
                if (e2 <= adx) { err += adx; y0 += stepy; }
            }
        } else if (t == tool::Rect || t == tool::FilledRect) {  // Rect
            int minx = std::min(drag.shape_sx, epx), maxx = std::max(drag.shape_sx, epx);
            int miny = std::min(drag.shape_sy, epy), maxy = std::max(drag.shape_sy, epy);
            if (t == tool::FilledRect) {
                for (int y = miny; y <= maxy; y++)
                    for (int x = minx; x <= maxx; x++)
                        draw_px(x, y);
            } else {
                for (int x = minx; x <= maxx; x++) { draw_px(x, miny); draw_px(x, maxy); }
                for (int y = miny + 1; y < maxy; y++) { draw_px(minx, y); draw_px(maxx, y); }
            }
        } else if (t == tool::Circle || t == tool::FilledCircle) {  // Ellipse — shares rasterize_ellipse() with commit
            raster::rasterize_ellipse(drag.shape_sx, drag.shape_sy, epx, epy, t == tool::FilledCircle,
                              [&](int x, int y) { draw_px(x, y); });
        } else if (t == tool::RectSelect) {  // Select — marching ants (vector UI overlay, not a drawing output)
            float ssx = cur_origin.x + static_cast<float>(drag.shape_sx) * z, ssy = cur_origin.y + static_cast<float>(drag.shape_sy) * z;
            float sex = cur_origin.x + static_cast<float>(px + 1) * z,  sey = cur_origin.y + static_cast<float>(py + 1) * z;
            dl->AddRect({ssx,ssy},{sex,sey}, IM_COL32(255,255,255,200), 0, 0, 1.5f);
            dl->AddRect({ssx+1,ssy+1},{sex-1,sey-1}, IM_COL32(0,0,0,200), 0, 0, 1.0f);
        }
    }

    // Move tool — left-drag pans just like middle-mouse
    if (tools.active_tool == tool::Move && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        cs.pan.x += io.MouseDelta.x;
        cs.pan.y += io.MouseDelta.y;
    }

    // Symmetry helpers — call fn for the primary position plus all mirrored variants.
    // Defined here (before IsItemHovered and the mouse_in_win blocks) so both the
    // brush/fill path and the shape-commit path can reach them.
    auto sym_pt = [&](int x, int y, auto fn) {
        fn(x, y);
        if (tools.symmetry) {
            int SW = cs.width(), SH = cs.height();
            if (tools.symmetry_mode == 0 || tools.symmetry_mode == 2) fn(SW-1-x, y);
            if (tools.symmetry_mode == 1 || tools.symmetry_mode == 2) fn(x, SH-1-y);
            if (tools.symmetry_mode == 2) fn(SW-1-x, SH-1-y);
        }
    };
    auto sym_seg = [&](int x0, int y0, int x1, int y1, auto fn) {
        fn(x0, y0, x1, y1);
        if (tools.symmetry) {
            int SW = cs.width(), SH = cs.height();
            if (tools.symmetry_mode == 0 || tools.symmetry_mode == 2) fn(SW-1-x0, y0, SW-1-x1, y1);
            if (tools.symmetry_mode == 1 || tools.symmetry_mode == 2) fn(x0, SH-1-y0, x1, SH-1-y1);
            if (tools.symmetry_mode == 2) fn(SW-1-x0, SH-1-y0, SW-1-x1, SH-1-y1);
        }
    };

    if (ImGui::IsItemHovered()) {
        if (io.MouseWheel != 0.0f) {
            float dir = io.MouseWheel > 0.0f ? 1.0f : -1.0f;
            float new_zoom;
            if (dir > 0.0f && cs.zoom >= 8.0f)
                new_zoom = std::ceil((cs.zoom + 1.0f) / 4.0f) * 4.0f;
            else if (dir < 0.0f && cs.zoom > 8.0f)
                new_zoom = std::floor((cs.zoom - 1.0f) / 4.0f) * 4.0f;
            else
                new_zoom = cs.zoom + dir;
            new_zoom = std::clamp(new_zoom, 1.0f, 128.0f);
            if (new_zoom != cs.zoom) {
                // Keep the canvas pixel under the cursor fixed in screen space
                cs.pan.x = io.MousePos.x - (io.MousePos.x - base.x - cs.pan.x) / cs.zoom * new_zoom - base.x;
                cs.pan.y = io.MousePos.y - (io.MousePos.y - base.y - cs.pan.y) / cs.zoom * new_zoom - base.y;
                cs.zoom  = new_zoom;
                Log("Zoom: %.0fx", cs.zoom);
                // Recompute after zoom/pan change
                cur_origin = {std::round(base.x + cs.pan.x), std::round(base.y + cs.pan.y)};
                px         = (int)((io.MousePos.x - cur_origin.x) / cs.zoom);
                py         = (int)((io.MousePos.y - cur_origin.y) / cs.zoom);
            }
        }

        uint32_t color = (tools.active_tool == tool::Eraser) ? 0x00000000u : ImGui::ColorConvertFloat4ToU32(palette.primary_color);

        if (tools.active_tool == tool::Fill) {
            was_painting = false;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (cs.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    cs.push_snapshot();
                    Log("Flood fill at (%d,%d) color #%08X", px, py, color);
                    sym_pt(px, py, [&](int x, int y) { raster::flood_fill(cs.active(), x, y, color); });
                    cs.rebuild_composite();
                }
            }
        } else if ((tools.active_tool >= tool::Line && tools.active_tool <= tool::FilledCircle) ||
                   tools.active_tool == tool::Move || tools.active_tool == tool::RectSelect) {
            // Drag-start handled by the unified window-rect block below
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
        } else if (tools.active_tool == tool::ColorPicker) {
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && cs.active().in_bounds(px, py)) {
                uint32_t picked = cs.active().get(px, py);
                palette.primary_color = ImGui::ColorConvertU32ToFloat4(picked);
                Log("Color pick at (%d,%d) #%08X", px, py, picked);
            }
        } else {
            bool is_painting = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            if (is_painting && !was_painting) {
                if (cs.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    cs.push_snapshot();
                    Log("Stroke start at (%d,%d) tool=%d size=%d", px, py, tools.active_tool, tools.brush_size);
                }
            }
            if (is_painting && !cs.active_layer_locked()) {
                if (last_px.x < 0.0f)
                    sym_pt(px, py, [&](int x, int y) {
                        raster::paint_pixel(cs.active(), x, y, color, tools.brush_size, tools.circle_brush);
                    });
                else
                    sym_seg((int)last_px.x, (int)last_px.y, px, py, [&](int x0, int y0, int x1, int y1) {
                        raster::bresenham(cs.active(), x0, y0, x1, y1, color, tools.brush_size, tools.circle_brush);
                    });
                last_px = {(float)px, (float)py};
                cs.rebuild_composite();
            } else if (!is_painting) {
                last_px = {-1.0f, -1.0f};
            }
            was_painting = is_painting;
        }
    } else {
        // Mouse left the canvas — reset brush state but keep shape/handle drags alive
        last_px      = {-1.0f, -1.0f};
        was_painting = false;
    }

    // Unified shape/selection click handler — single source of truth for tools 3-7 and 9.
    // Fires anywhere in the canvas window (over the image OR the margin when zoomed out),
    // so drags can start/move/lift off-canvas. IsWindowHovered is unreliable in docked
    // layouts, so we use a direct screen-rect check. The IsItemHovered block above only
    // resets brush state for these tools, so there is no double-handling.
    {
        ImVec2 wpos  = ImGui::GetWindowPos();
        ImVec2 wsize = ImGui::GetWindowSize();
        bool mouse_in_win = io.MousePos.x >= wpos.x && io.MousePos.x < wpos.x + wsize.x
                         && io.MousePos.y >= wpos.y && io.MousePos.y < wpos.y + wsize.y;
        tools.mouse_over_canvas = mouse_in_win;
        if (mouse_in_win && !any_popup) {
        if (tools.active_tool >= tool::Line && tools.active_tool <= tool::FilledCircle) {
            if (!drag.shape_dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (cs.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    cs.push_snapshot();
                    drag.shape_sx       = px;
                    drag.shape_sy       = py;
                    drag.shape_dragging = true;
                    Log("Shape start at (%d,%d) tool=%d", px, py, tools.active_tool);
                }
            }
        } else if (tools.active_tool == tool::RectSelect) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Priority 1: handle hit
                int hit_h = -1;
                if (sel.active) {
                    for (int i = 0; i < 8; i++) {
                        if (std::abs(io.MousePos.x - hpos[i].x) <= 5.0f &&
                            std::abs(io.MousePos.y - hpos[i].y) <= 5.0f) {
                            hit_h = i;
                            break;
                        }
                    }
                }

                if (hit_h >= 0) {
                    // Start handle drag
                    drag.handle_dragging  = true;
                    drag.active_handle    = hit_h;
                    drag.handle_start_x0  = sel.x0;
                    drag.handle_start_y0  = sel.y0;
                    drag.handle_start_x1  = sel.x1;
                    drag.handle_start_y1  = sel.y1;
                } else if (sel.floating) {
                    // Priority 2: re-drag or commit floating
                    bool inside_float = px >= sel.float_x && px < sel.float_x + sel.float_w
                                     && py >= sel.float_y && py < sel.float_y + sel.float_h;
                    if (inside_float) {
                        drag.float_drag_ox  = px - sel.float_x;
                        drag.float_drag_oy  = py - sel.float_y;
                        drag.shape_dragging = true;
                        drag.shape_sx = px; drag.shape_sy = py;
                    } else {
                        // Click outside floating content — commit and start new selection
                        commit_floating(cs, sel);
                        sel.active          = false;
                        drag.shape_sx       = px;
                        drag.shape_sy       = py;
                        drag.shape_dragging = true;
                    }
                } else if (sel.active &&
                           px >= sel.x0 && px <= sel.x1 &&
                           py >= sel.y0 && py <= sel.y1) {
                    // Priority 3: lift selection into floating
                    if (cs.active_layer_locked()) {
                        Log("Layer locked");
                    } else {
                        lift_selection(cs, sel);
                        drag.float_drag_ox  = px - sel.float_x;
                        drag.float_drag_oy  = py - sel.float_y;
                        drag.shape_dragging = true;
                        drag.shape_sx = px; drag.shape_sy = py;
                    }
                } else {
                    // Priority 4: start new selection drag
                    sel.active          = false;
                    drag.shape_sx       = px;
                    drag.shape_sy       = py;
                    drag.shape_dragging = true;
                }
            }
        }
        } // if (mouse_in_win && !IsItemHovered())
    } // margin block

    // Update floating selection position during drag (continues outside canvas bounds)
    if (sel.floating && drag.shape_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        sel.float_x = px - drag.float_drag_ox;
        sel.float_y = py - drag.float_drag_oy;
        sel.x0 = sel.float_x;
        sel.y0 = sel.float_y;
        sel.x1 = sel.float_x + sel.float_w - 1;
        sel.y1 = sel.float_y + sel.float_h - 1;
        sel.active = true;
    }

    // Update handle drag bounds while mouse is held — selection may extend beyond the canvas
    if (drag.handle_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        int nx0 = drag.handle_start_x0, ny0 = drag.handle_start_y0;
        int nx1 = drag.handle_start_x1, ny1 = drag.handle_start_y1;
        if (k_hmoves[drag.active_handle][0]) nx0 = px;
        if (k_hmoves[drag.active_handle][1]) ny0 = py;
        if (k_hmoves[drag.active_handle][2]) nx1 = px;
        if (k_hmoves[drag.active_handle][3]) ny1 = py;
        sel.x0 = std::min(nx0, nx1);
        sel.y0 = std::min(ny0, ny1);
        sel.x1 = std::max(nx0, nx1);
        sel.y1 = std::max(ny0, ny1);
        if (sel.floating) {
            sel.float_x = sel.x0;
            sel.float_y = sel.y0;
        }
    }

    // Commit handle drag on mouse release — scale floating content if dimensions changed
    if (drag.handle_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (sel.floating) {
            int new_w = sel.x1 - sel.x0 + 1;
            int new_h = sel.y1 - sel.y0 + 1;
            if (new_w > 0 && new_h > 0 &&
                (new_w != sel.float_w || new_h != sel.float_h)) {
                std::vector<uint32_t> scaled;
                raster::nn_scale(sel.float_pixels, sel.float_w, sel.float_h, scaled, new_w, new_h);
                sel.float_pixels = std::move(scaled);
                sel.float_w = new_w;
                sel.float_h = new_h;
            }
            sel.float_x = sel.x0;
            sel.float_y = sel.y0;
        }
        drag.handle_dragging = false;
        drag.active_handle   = -1;
        Log("Handle resize: sel=(%d,%d)-(%d,%d)", sel.x0, sel.y0, sel.x1, sel.y1);
    }

    // Commit shape on mouse release regardless of hover state
    if (drag.shape_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Log("Shape commit tool=%d (%d,%d)->(%d,%d)", tools.active_tool, drag.shape_sx, drag.shape_sy, px, py);
        if (tools.active_tool == tool::RectSelect) {
            if (sel.floating) {
                // Keep floating after mouse release — commit happens on tool switch or Escape
            } else {
                int ax = std::min(drag.shape_sx, px);
                int ay = std::min(drag.shape_sy, py);
                int bx = std::max(drag.shape_sx, px);
                int by = std::max(drag.shape_sy, py);
                sel.active = (ax != bx || ay != by);
                if (sel.active) { sel.x0 = ax; sel.y0 = ay; sel.x1 = bx; sel.y1 = by; }
            }
        } else {
            uint32_t color = ImGui::ColorConvertFloat4ToU32(palette.primary_color);
            int cpx = px, cpy = py;
            int ct  = tools.active_tool;
            if (io.KeyShift && tool::is_shape(ct)) {
                int ddx = px - drag.shape_sx, ddy = py - drag.shape_sy;
                int ax = std::abs(ddx), ay = std::abs(ddy);
                int d = (std::abs(ax - ay) <= 1) ? std::max(ax, ay) : std::min(ax, ay);
                cpx   = drag.shape_sx + (ddx < 0 ? -d : d);
                cpy   = drag.shape_sy + (ddy < 0 ? -d : d);
            }
            sym_seg(drag.shape_sx, drag.shape_sy, cpx, cpy, [&](int x0, int y0, int x1, int y1) {
                switch (ct) {
                case tool::Line:         raster::bresenham(cs.active(), x0, y0, x1, y1, color, tools.brush_size, tools.circle_brush); break;
                case tool::Rect:         raster::draw_rect(cs.active(), x0, y0, x1, y1, color, false); break;
                case tool::FilledRect:   raster::draw_rect(cs.active(), x0, y0, x1, y1, color, true);  break;
                case tool::Circle:       raster::draw_ellipse(cs.active(), x0, y0, x1, y1, color, false); break;
                case tool::FilledCircle: raster::draw_ellipse(cs.active(), x0, y0, x1, y1, color, true);  break;
                default: break;
                }
            });
            cs.rebuild_composite();
        }
        drag.shape_dragging = false;
    }

    ImGui::End();
}
