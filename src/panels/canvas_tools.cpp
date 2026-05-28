#include "canvas_tools.h"
#include "raster.h"
#include "log.h"
#include "imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>

void handle_canvas_tool_input(CanvasToolInputCtx& ctx) {
    CanvasState&    cs      = ctx.cs;
    ToolsState&     tools   = ctx.tools;
    SelectionState& sel     = ctx.sel;
    PaletteState&   palette = ctx.palette;
    DocRenderState& drs     = ctx.drs;
    ImDrawList*     dl      = ctx.dl;
    ImVec2&         origin  = ctx.origin;
    int&            px      = ctx.px;
    int&            py      = ctx.py;
    ImGuiIO&        io      = ImGui::GetIO();

    // Shape preview overlay — pixel-exact, mirrors the committed drawing algorithms
    if (drs.drag.shape_dragging && !sel.floating) {
        ImU32 prev_col = (ImGui::ColorConvertFloat4ToU32(palette.primary_color) & 0x00FFFFFFu) | 0xCC000000u;
        float z = cs.zoom;

        auto draw_px_base = [&](int cx, int cy) {
            if (cx < 0 || cx >= cs.width() || cy < 0 || cy >= cs.height()) return;
            float sx = origin.x + static_cast<float>(cx) * z;
            float sy = origin.y + static_cast<float>(cy) * z;
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
            int ddx = px - drs.drag.shape_sx, ddy = py - drs.drag.shape_sy;
            int ax = std::abs(ddx), ay = std::abs(ddy);
            // 1-pixel hysteresis: when deltas are ≤1 apart (pen near 45°), round up
            // instead of down so 1-pixel tablet jitter doesn't oscillate between N and N+1.
            int d = (std::abs(ax - ay) <= 1) ? std::max(ax, ay) : std::min(ax, ay);
            epx   = drs.drag.shape_sx + (ddx < 0 ? -d : d);
            epy   = drs.drag.shape_sy + (ddy < 0 ? -d : d);
        }
        if (t == tool::Line) {  // Line — Bresenham + brush stamp
            int x0 = drs.drag.shape_sx, y0 = drs.drag.shape_sy, x1 = px, y1 = py;
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
            int minx = std::min(drs.drag.shape_sx, epx), maxx = std::max(drs.drag.shape_sx, epx);
            int miny = std::min(drs.drag.shape_sy, epy), maxy = std::max(drs.drag.shape_sy, epy);
            if (t == tool::FilledRect) {
                for (int y = miny; y <= maxy; y++)
                    for (int x = minx; x <= maxx; x++)
                        draw_px(x, y);
            } else {
                for (int x = minx; x <= maxx; x++) { draw_px(x, miny); draw_px(x, maxy); }
                for (int y = miny + 1; y < maxy; y++) { draw_px(minx, y); draw_px(maxx, y); }
            }
        } else if (t == tool::Circle || t == tool::FilledCircle) {  // Ellipse — shares rasterize_ellipse() with commit
            raster::rasterize_ellipse(drs.drag.shape_sx, drs.drag.shape_sy, epx, epy, t == tool::FilledCircle,
                              [&](int x, int y) { draw_px(x, y); });
        } else if (t == tool::RectSelect) {  // Select — marching ants (vector UI overlay, not a drawing output)
            float ssx = origin.x + static_cast<float>(drs.drag.shape_sx) * z, ssy = origin.y + static_cast<float>(drs.drag.shape_sy) * z;
            float sex = origin.x + static_cast<float>(px + 1) * z,  sey = origin.y + static_cast<float>(py + 1) * z;
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
                cs.pan.x = io.MousePos.x - (io.MousePos.x - ctx.base.x - cs.pan.x) / cs.zoom * new_zoom - ctx.base.x;
                cs.pan.y = io.MousePos.y - (io.MousePos.y - ctx.base.y - cs.pan.y) / cs.zoom * new_zoom - ctx.base.y;
                cs.zoom  = new_zoom;
                Log("Zoom: %.0fx", cs.zoom);
                // Recompute after zoom/pan change
                origin = {std::round(ctx.base.x + cs.pan.x), std::round(ctx.base.y + cs.pan.y)};
                px     = (int)((io.MousePos.x - origin.x) / cs.zoom);
                py     = (int)((io.MousePos.y - origin.y) / cs.zoom);
            }
        }

        uint32_t color = (tools.active_tool == tool::Eraser) ? 0x00000000u : ImGui::ColorConvertFloat4ToU32(palette.primary_color);

        if (tools.active_tool == tool::Fill) {
            drs.was_painting = false;
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
            drs.was_painting = false;
            drs.last_px      = {-1.0f, -1.0f};
        } else if (tools.active_tool == tool::ColorPicker) {
            drs.was_painting = false;
            drs.last_px      = {-1.0f, -1.0f};
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && cs.active().in_bounds(px, py)) {
                uint32_t picked = cs.active().get(px, py);
                palette.primary_color = ImGui::ColorConvertU32ToFloat4(picked);
                Log("Color pick at (%d,%d) #%08X", px, py, picked);
            }
        } else if (tools.active_tool == tool::ColorSelect) {
            drs.was_painting = false;
            drs.last_px      = {-1.0f, -1.0f};
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (sel.floating) {
                    commit_floating(cs, sel);
                    drs.drag.handle_dragging = false;
                    drs.drag.active_handle   = -1;
                }
                if (cs.active().in_bounds(px, py)) {
                    auto result = raster::color_select(cs.active(), px, py,
                                                       tools.color_select_contiguous);
                    if (result.any) {
                        sel.active = true;
                        sel.x0 = result.x0; sel.y0 = result.y0;
                        sel.x1 = result.x1; sel.y1 = result.y1;
                        sel.mask = std::move(result.mask);
                        Log("Color select at (%d,%d): bbox (%d,%d)-(%d,%d)",
                            px, py, sel.x0, sel.y0, sel.x1, sel.y1);
                    } else {
                        sel.active = false;
                        sel.mask.clear();
                    }
                }
            }
        } else {
            bool is_painting = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            if (is_painting && !drs.was_painting) {
                if (cs.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    cs.push_snapshot();
                    Log("Stroke start at (%d,%d) tool=%d size=%d", px, py, tools.active_tool, tools.brush_size);
                }
            }
            if (is_painting && !cs.active_layer_locked()) {
                if (drs.last_px.x < 0.0f)
                    sym_pt(px, py, [&](int x, int y) {
                        raster::paint_pixel(cs.active(), x, y, color, tools.brush_size, tools.circle_brush);
                    });
                else
                    sym_seg((int)drs.last_px.x, (int)drs.last_px.y, px, py, [&](int x0, int y0, int x1, int y1) {
                        raster::bresenham(cs.active(), x0, y0, x1, y1, color, tools.brush_size, tools.circle_brush);
                    });
                drs.last_px = {(float)px, (float)py};
                cs.rebuild_composite();
            } else if (!is_painting) {
                drs.last_px = {-1.0f, -1.0f};
            }
            drs.was_painting = is_painting;
        }
    } else {
        // Mouse left the canvas — reset brush state but keep shape/handle drags alive
        drs.last_px      = {-1.0f, -1.0f};
        drs.was_painting = false;
    }

    // Unified shape/selection click handler — single source of truth for tools 3-7 and 9.
    // Fires anywhere in the canvas window (over the image OR the margin when zoomed out),
    // so drags can start/move/lift off-canvas. IsWindowHovered is unreliable in docked
    // layouts, so we use a direct screen-rect check. The IsItemHovered block above only
    // resets brush state for these tools, so there is no double-handling.
    {
        tools.mouse_over_canvas = ctx.mouse_in_win;
        if (ctx.mouse_in_win && !ctx.any_popup) {
        if (tools.active_tool >= tool::Line && tools.active_tool <= tool::FilledCircle) {
            if (!drs.drag.shape_dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (cs.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    cs.push_snapshot();
                    drs.drag.shape_sx       = px;
                    drs.drag.shape_sy       = py;
                    drs.drag.shape_dragging = true;
                    Log("Shape start at (%d,%d) tool=%d", px, py, tools.active_tool);
                }
            }
        } else if (tools.active_tool == tool::RectSelect) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Priority 1: handle hit
                int hit_h = -1;
                if (sel.active) {
                    for (int i = 0; i < 8; i++) {
                        if (std::abs(io.MousePos.x - ctx.hpos[i].x) <= 5.0f &&
                            std::abs(io.MousePos.y - ctx.hpos[i].y) <= 5.0f) {
                            hit_h = i;
                            break;
                        }
                    }
                }

                if (hit_h >= 0) {
                    // Start handle drag
                    drs.drag.handle_dragging  = true;
                    drs.drag.active_handle    = hit_h;
                    drs.drag.handle_start_x0  = sel.x0;
                    drs.drag.handle_start_y0  = sel.y0;
                    drs.drag.handle_start_x1  = sel.x1;
                    drs.drag.handle_start_y1  = sel.y1;
                } else if (sel.floating) {
                    // Priority 2: re-drag or commit floating
                    bool inside_float = px >= sel.float_x && px < sel.float_x + sel.float_w
                                     && py >= sel.float_y && py < sel.float_y + sel.float_h;
                    if (inside_float) {
                        drs.drag.float_drag_ox  = px - sel.float_x;
                        drs.drag.float_drag_oy  = py - sel.float_y;
                        drs.drag.shape_dragging = true;
                        drs.drag.shape_sx = px; drs.drag.shape_sy = py;
                    } else {
                        // Click outside floating content — commit and start new selection
                        commit_floating(cs, sel);
                        sel.active              = false;
                        sel.mask.clear();
                        drs.drag.shape_sx       = px;
                        drs.drag.shape_sy       = py;
                        drs.drag.shape_dragging = true;
                    }
                } else if (sel.active &&
                           px >= sel.x0 && px <= sel.x1 &&
                           py >= sel.y0 && py <= sel.y1) {
                    // Priority 3: lift selection into floating
                    if (cs.active_layer_locked()) {
                        Log("Layer locked");
                    } else {
                        lift_selection(cs, sel);
                        drs.drag.float_drag_ox  = px - sel.float_x;
                        drs.drag.float_drag_oy  = py - sel.float_y;
                        drs.drag.shape_dragging = true;
                        drs.drag.shape_sx = px; drs.drag.shape_sy = py;
                    }
                } else {
                    // Priority 4: start new selection drag
                    sel.active          = false;
                    sel.mask.clear();
                    drs.drag.shape_sx   = px;
                    drs.drag.shape_sy   = py;
                    drs.drag.shape_dragging = true;
                }
            }
        }
        } // if (mouse_in_win && !any_popup)
    }

    // Update floating selection position during drag (continues outside canvas bounds)
    if (sel.floating && drs.drag.shape_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        sel.float_x = px - drs.drag.float_drag_ox;
        sel.float_y = py - drs.drag.float_drag_oy;
        sel.x0 = sel.float_x;
        sel.y0 = sel.float_y;
        sel.x1 = sel.float_x + sel.float_w - 1;
        sel.y1 = sel.float_y + sel.float_h - 1;
        sel.active = true;
    }

    auto sync_float_pos = [&]() { sel.float_x = sel.x0; sel.float_y = sel.y0; };

    // Update handle drag bounds while mouse is held — selection may extend beyond the canvas
    if (drs.drag.handle_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        assert(drs.drag.active_handle >= 0 && drs.drag.active_handle < 8);
        int nx0 = drs.drag.handle_start_x0, ny0 = drs.drag.handle_start_y0;
        int nx1 = drs.drag.handle_start_x1, ny1 = drs.drag.handle_start_y1;
        if (k_hmoves[drs.drag.active_handle][0]) nx0 = px;
        if (k_hmoves[drs.drag.active_handle][1]) ny0 = py;
        if (k_hmoves[drs.drag.active_handle][2]) nx1 = px;
        if (k_hmoves[drs.drag.active_handle][3]) ny1 = py;
        sel.x0 = std::min(nx0, nx1);
        sel.y0 = std::min(ny0, ny1);
        sel.x1 = std::max(nx0, nx1);
        sel.y1 = std::max(ny0, ny1);
        if (sel.floating) sync_float_pos();
    }

    // Commit handle drag on mouse release — scale floating content if dimensions changed
    if (drs.drag.handle_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
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
            sync_float_pos();
        }
        drs.drag.handle_dragging = false;
        drs.drag.active_handle   = -1;
        Log("Handle resize: sel=(%d,%d)-(%d,%d)", sel.x0, sel.y0, sel.x1, sel.y1);
    }

    // Commit shape on mouse release regardless of hover state
    if (drs.drag.shape_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Log("Shape commit tool=%d (%d,%d)->(%d,%d)", tools.active_tool, drs.drag.shape_sx, drs.drag.shape_sy, px, py);
        if (tools.active_tool == tool::RectSelect) {
            if (sel.floating) {
                // Keep floating after mouse release — commit happens on tool switch or Escape
            } else {
                int ax = std::min(drs.drag.shape_sx, px);
                int ay = std::min(drs.drag.shape_sy, py);
                int bx = std::max(drs.drag.shape_sx, px);
                int by = std::max(drs.drag.shape_sy, py);
                sel.active = (ax != bx || ay != by);
                if (sel.active) { sel.x0 = ax; sel.y0 = ay; sel.x1 = bx; sel.y1 = by; }
            }
        } else {
            uint32_t color = ImGui::ColorConvertFloat4ToU32(palette.primary_color);
            int cpx = px, cpy = py;
            int ct  = tools.active_tool;
            if (io.KeyShift && tool::is_shape(ct)) {
                int ddx = px - drs.drag.shape_sx, ddy = py - drs.drag.shape_sy;
                int ax = std::abs(ddx), ay = std::abs(ddy);
                int d = (std::abs(ax - ay) <= 1) ? std::max(ax, ay) : std::min(ax, ay);
                cpx   = drs.drag.shape_sx + (ddx < 0 ? -d : d);
                cpy   = drs.drag.shape_sy + (ddy < 0 ? -d : d);
            }
            sym_seg(drs.drag.shape_sx, drs.drag.shape_sy, cpx, cpy, [&](int x0, int y0, int x1, int y1) {
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
        drs.drag.shape_dragging = false;
    }
}
