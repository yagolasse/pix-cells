#include "canvas_panel.h"
#include "raster.h"
#include "log.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

static void lift_selection(CanvasState& cs, SelectionState& sel) {
    cs.push_snapshot();
    int sw = sel.x1 - sel.x0 + 1;
    int sh = sel.y1 - sel.y0 + 1;
    sel.float_pixels.resize(sw * sh);
    sel.float_w      = sw;
    sel.float_h      = sh;
    sel.float_x      = sel.x0;
    sel.float_y      = sel.y0;
    sel.float_orig_x = sel.x0;
    sel.float_orig_y = sel.y0;
    Canvas& c = cs.active();
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++) {
            sel.float_pixels[y * sw + x] = c.get(sel.x0 + x, sel.y0 + y);
            c.set(sel.x0 + x, sel.y0 + y, 0x00000000);
        }
    sel.floating = true;
    cs.rebuild_composite();
    Log("Lift selection: %dx%d", sw, sh);
}

static void commit_floating(CanvasState& cs, SelectionState& sel) {
    Canvas& c = cs.active();
    for (int y = 0; y < sel.float_h; y++)
        for (int x = 0; x < sel.float_w; x++) {
            uint32_t pv = sel.float_pixels[y * sel.float_w + x];
            if (((pv >> 24) & 0xFF) > 0)
                c.set(sel.float_x + x, sel.float_y + y, pv);
        }
    sel.active   = false;
    sel.floating = false;
    sel.float_pixels.clear();
    cs.rebuild_composite();
    Log("Commit float to (%d,%d)", sel.float_x, sel.float_y);
}

// Handle move table: {moves_x0, moves_y0, moves_x1, moves_y1}
// Handles: TL=0 TM=1 TR=2 RM=3 BR=4 BM=5 BL=6 LM=7
static constexpr bool k_hmoves[8][4] = {
    {1,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,1,0},
    {0,0,1,1}, {0,0,0,1}, {1,0,0,1}, {1,0,0,0}
};

void panels::DrawCanvas(CanvasState& cs, const ToolsState& tools, PaletteState& palette, SelectionState& sel) {
    static GLuint texture = 0;
    static int tex_w = 0, tex_h = 0;
    static ImVec2 last_px       = {-1.0f, -1.0f};
    static bool was_painting    = false;
    static bool shape_dragging  = false;
    static int shape_sx = -1, shape_sy = -1;
    static int float_drag_ox = 0, float_drag_oy = 0;
    static int active_handle    = -1;
    static bool handle_dragging = false;
    static int handle_start_x0 = 0, handle_start_y0 = 0;
    static int handle_start_x1 = 0, handle_start_y1 = 0;

    // (Re)create texture on first call or if canvas dimensions changed
    if (texture == 0 || tex_w != cs.width() || tex_h != cs.height()) {
        if (texture != 0)
            glDeleteTextures(1, &texture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.width(), cs.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     cs.composite.data());
        tex_w    = cs.width();
        tex_h    = cs.height();
        cs.dirty = false;
    } else if (cs.dirty) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                        cs.composite.data());
        cs.dirty = false;
    }

    ImGui::Begin("Canvas");

    // Commit floating selection automatically when tool changes away from rect select
    if (sel.floating && tools.active_tool != tool::RectSelect) {
        commit_floating(cs, sel);
        handle_dragging = false;
        active_handle   = -1;
    }

    ImGuiIO& io = ImGui::GetIO();

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
    ImVec2 origin  = {base.x + cs.pan.x, base.y + cs.pan.y};
    float W        = cs.width() * cs.zoom;
    float H        = cs.height() * cs.zoom;

    // Checkerboard background — visible through transparent pixels
    {
        float cell = cs.checker_size * cs.zoom;
        ImU32 col1 = ImGui::ColorConvertFloat4ToU32(cs.checker_color1);
        ImU32 col2 = ImGui::ColorConvertFloat4ToU32(cs.checker_color2);
        dl->PushClipRect({origin.x, origin.y}, {origin.x + W, origin.y + H}, true);
        for (int row = 0; row * cell < H; row++)
            for (int col = 0; col * cell < W; col++) {
                float cx = origin.x + col * cell;
                float cy = origin.y + row * cell;
                ImU32 c  = (row + col) % 2 ? col2 : col1;
                dl->AddRectFilled({cx, cy}, {cx + cell, cy + cell}, c);
            }
        dl->PopClipRect();
    }

    // ImGui v1.92+ binds a linear sampler that overrides texture parameters — switch to nearest.
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if (pio.DrawCallback_SetSamplerNearest)
        dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);

    ImGui::SetCursorScreenPos(origin);
    ImGui::Image((ImTextureID)(uintptr_t)texture, {W, H});

    if (pio.DrawCallback_SetSamplerLinear)
        dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);

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
    ImVec2 hpos[8] = {};
    if (sel.active && tools.active_tool == tool::RectSelect) {
        float sx0 = origin.x + sel.x0 * cs.zoom;
        float sy0 = origin.y + sel.y0 * cs.zoom;
        float sx1 = origin.x + (sel.x1 + 1) * cs.zoom;
        float sy1 = origin.y + (sel.y1 + 1) * cs.zoom;
        float scx = (sx0 + sx1) * 0.5f, scy = (sy0 + sy1) * 0.5f;
        hpos[0]={sx0,sy0}; hpos[1]={scx,sy0}; hpos[2]={sx1,sy0}; hpos[3]={sx1,scy};
        hpos[4]={sx1,sy1}; hpos[5]={scx,sy1}; hpos[6]={sx0,sy1}; hpos[7]={sx0,scy};
        for (int i = 0; i < 8; i++) {
            bool is_active = handle_dragging && active_handle == i;
            ImU32 hcol = is_active ? IM_COL32(255,200,50,255) : IM_COL32(255,255,255,220);
            dl->AddRectFilled({hpos[i].x-3.5f,hpos[i].y-3.5f},{hpos[i].x+3.5f,hpos[i].y+3.5f}, hcol);
            dl->AddRect({hpos[i].x-3.5f,hpos[i].y-3.5f},{hpos[i].x+3.5f,hpos[i].y+3.5f}, IM_COL32(0,0,0,200));
        }
    }

    // Tool input — compute canvas coords early so shape commit can use them outside hovered block
    ImVec2 cur_origin = {base.x + cs.pan.x, base.y + cs.pan.y};
    int px            = (int)((io.MousePos.x - cur_origin.x) / cs.zoom);
    int py            = (int)((io.MousePos.y - cur_origin.y) / cs.zoom);

    // Shape preview overlay — pixel-exact, mirrors the committed drawing algorithms
    if (shape_dragging && !sel.floating) {
        ImU32 prev_col = (ImGui::ColorConvertFloat4ToU32(palette.primary_color) & 0x00FFFFFFu) | 0xCC000000u;
        float z = cs.zoom;

        auto draw_px = [&](int cx, int cy) {
            if (cx < 0 || cx >= cs.width() || cy < 0 || cy >= cs.height()) return;
            float sx = cur_origin.x + cx * z;
            float sy = cur_origin.y + cy * z;
            dl->AddRectFilled({sx, sy}, {sx + z, sy + z}, prev_col);
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
            int ddx = px - shape_sx, ddy = py - shape_sy;
            int d   = std::min(std::abs(ddx), std::abs(ddy));
            epx     = shape_sx + (ddx < 0 ? -d : d);
            epy     = shape_sy + (ddy < 0 ? -d : d);
        }
        if (t == tool::Line) {  // Line — Bresenham + brush stamp
            int x0 = shape_sx, y0 = shape_sy, x1 = px, y1 = py;
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
            int minx = std::min(shape_sx, epx), maxx = std::max(shape_sx, epx);
            int miny = std::min(shape_sy, epy), maxy = std::max(shape_sy, epy);
            if (t == tool::FilledRect) {
                for (int y = miny; y <= maxy; y++)
                    for (int x = minx; x <= maxx; x++)
                        draw_px(x, y);
            } else {
                for (int x = minx; x <= maxx; x++) { draw_px(x, miny); draw_px(x, maxy); }
                for (int y = miny + 1; y < maxy; y++) { draw_px(minx, y); draw_px(maxx, y); }
            }
        } else if (t == tool::Circle || t == tool::FilledCircle) {  // Ellipse — shares rasterize_ellipse() with commit
            raster::rasterize_ellipse(shape_sx, shape_sy, epx, epy, t == tool::FilledCircle,
                              [&](int x, int y) { draw_px(x, y); });
        } else if (t == tool::RectSelect) {  // Select — marching ants (vector UI overlay, not a drawing output)
            float ssx = cur_origin.x + shape_sx * z, ssy = cur_origin.y + shape_sy * z;
            float sex = cur_origin.x + (px + 1) * z,  sey = cur_origin.y + (py + 1) * z;
            dl->AddRect({ssx,ssy},{sex,sey}, IM_COL32(255,255,255,200), 0, 0, 1.5f);
            dl->AddRect({ssx+1,ssy+1},{sex-1,sey-1}, IM_COL32(0,0,0,200), 0, 0, 1.0f);
        }
    }

    // Move tool — left-drag pans just like middle-mouse
    if (tools.active_tool == tool::Move && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        cs.pan.x += io.MouseDelta.x;
        cs.pan.y += io.MouseDelta.y;
    }

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
            new_zoom = std::clamp(new_zoom, 1.0f, 32.0f);
            if (new_zoom != cs.zoom) {
                // Keep the canvas pixel under the cursor fixed in screen space
                cs.pan.x = io.MousePos.x - (io.MousePos.x - base.x - cs.pan.x) / cs.zoom * new_zoom - base.x;
                cs.pan.y = io.MousePos.y - (io.MousePos.y - base.y - cs.pan.y) / cs.zoom * new_zoom - base.y;
                cs.zoom  = new_zoom;
                Log("Zoom: %.0fx", cs.zoom);
                // Recompute after zoom/pan change
                cur_origin = {base.x + cs.pan.x, base.y + cs.pan.y};
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
                    raster::flood_fill(cs.active(), px, py, color);
                    cs.rebuild_composite();
                }
            }
        } else if (tools.active_tool >= tool::Line && tools.active_tool <= tool::FilledCircle) {
            // Shape drag-start handled by the unified window-rect block below
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
        } else if (tools.active_tool == tool::Move) {
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
        } else if (tools.active_tool == tool::RectSelect) {
            // Selection click handled by the unified window-rect block below
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
        } else if (tools.active_tool == tool::ColorPicker) {
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && cs.active().in_bounds(px, py)) {
                palette.primary_color = ImGui::ColorConvertU32ToFloat4(cs.active().get(px, py));
                Log("Color pick at (%d,%d) #%08X", px, py, cs.active().get(px, py));
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
                    raster::paint_pixel(cs.active(), px, py, color, tools.brush_size, tools.circle_brush);
                else
                    raster::bresenham(cs.active(), (int)last_px.x, (int)last_px.y, px, py, color, tools.brush_size, tools.circle_brush);
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
        if (mouse_in_win) {
        if (tools.active_tool >= tool::Line && tools.active_tool <= tool::FilledCircle) {
            if (!shape_dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (cs.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    cs.push_snapshot();
                    shape_sx       = px;
                    shape_sy       = py;
                    shape_dragging = true;
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
                    handle_dragging  = true;
                    active_handle    = hit_h;
                    handle_start_x0  = sel.x0;
                    handle_start_y0  = sel.y0;
                    handle_start_x1  = sel.x1;
                    handle_start_y1  = sel.y1;
                } else if (sel.floating) {
                    // Priority 2: re-drag or commit floating
                    bool inside_float = px >= sel.float_x && px < sel.float_x + sel.float_w
                                     && py >= sel.float_y && py < sel.float_y + sel.float_h;
                    if (inside_float) {
                        float_drag_ox  = px - sel.float_x;
                        float_drag_oy  = py - sel.float_y;
                        shape_dragging = true;
                        shape_sx = px; shape_sy = py;
                    } else {
                        // Click outside floating content — commit and start new selection
                        commit_floating(cs, sel);
                        sel.active     = false;
                        shape_sx       = px;
                        shape_sy       = py;
                        shape_dragging = true;
                    }
                } else if (sel.active &&
                           px >= sel.x0 && px <= sel.x1 &&
                           py >= sel.y0 && py <= sel.y1) {
                    // Priority 3: lift selection into floating
                    if (cs.active_layer_locked()) {
                        Log("Layer locked");
                    } else {
                        lift_selection(cs, sel);
                        float_drag_ox  = px - sel.float_x;
                        float_drag_oy  = py - sel.float_y;
                        shape_dragging = true;
                        shape_sx = px; shape_sy = py;
                    }
                } else {
                    // Priority 4: start new selection drag
                    sel.active     = false;
                    shape_sx       = px;
                    shape_sy       = py;
                    shape_dragging = true;
                }
            }
        }
        } // if (mouse_in_win && !IsItemHovered())
    } // margin block

    // Update floating selection position during drag (continues outside canvas bounds)
    if (sel.floating && shape_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        sel.float_x = px - float_drag_ox;
        sel.float_y = py - float_drag_oy;
        sel.x0 = sel.float_x;
        sel.y0 = sel.float_y;
        sel.x1 = sel.float_x + sel.float_w - 1;
        sel.y1 = sel.float_y + sel.float_h - 1;
        sel.active = true;
    }

    // Update handle drag bounds while mouse is held — selection may extend beyond the canvas
    if (handle_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        int nx0 = handle_start_x0, ny0 = handle_start_y0;
        int nx1 = handle_start_x1, ny1 = handle_start_y1;
        if (k_hmoves[active_handle][0]) nx0 = px;
        if (k_hmoves[active_handle][1]) ny0 = py;
        if (k_hmoves[active_handle][2]) nx1 = px;
        if (k_hmoves[active_handle][3]) ny1 = py;
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
    if (handle_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
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
        handle_dragging = false;
        active_handle   = -1;
        Log("Handle resize: sel=(%d,%d)-(%d,%d)", sel.x0, sel.y0, sel.x1, sel.y1);
    }

    // Commit shape on mouse release regardless of hover state
    if (shape_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Log("Shape commit tool=%d (%d,%d)->(%d,%d)", tools.active_tool, shape_sx, shape_sy, px, py);
        if (tools.active_tool == tool::RectSelect) {
            if (sel.floating) {
                // Keep floating after mouse release — commit happens on tool switch or Escape
            } else {
                int ax = std::min(shape_sx, px);
                int ay = std::min(shape_sy, py);
                int bx = std::max(shape_sx, px);
                int by = std::max(shape_sy, py);
                sel.active = (ax != bx || ay != by);
                if (sel.active) { sel.x0 = ax; sel.y0 = ay; sel.x1 = bx; sel.y1 = by; }
            }
        } else {
            uint32_t color = ImGui::ColorConvertFloat4ToU32(palette.primary_color);
            int cpx = px, cpy = py;
            int ct  = tools.active_tool;
            if (io.KeyShift && tool::is_shape(ct)) {
                int ddx = px - shape_sx, ddy = py - shape_sy;
                int d   = std::min(std::abs(ddx), std::abs(ddy));
                cpx     = shape_sx + (ddx < 0 ? -d : d);
                cpy     = shape_sy + (ddy < 0 ? -d : d);
            }
            switch (ct) {
            case tool::Line:       raster::bresenham(cs.active(), shape_sx, shape_sy, px, py, color, tools.brush_size, tools.circle_brush); break;
            case tool::Rect:       raster::draw_rect(cs.active(), shape_sx, shape_sy, cpx, cpy, color, false); break;
            case tool::FilledRect: raster::draw_rect(cs.active(), shape_sx, shape_sy, cpx, cpy, color, true);  break;
            case tool::Circle:     raster::draw_ellipse(cs.active(), shape_sx, shape_sy, cpx, cpy, color, false); break;
            case tool::FilledCircle: raster::draw_ellipse(cs.active(), shape_sx, shape_sy, cpx, cpy, color, true);  break;
            }
            cs.rebuild_composite();
        }
        shape_dragging = false;
    }

    ImGui::End();
}
