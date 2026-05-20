#include "canvas_panel.h"
#include "log.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

static void paint_pixel(CanvasState& cs, int x, int y, uint32_t color, int brush_size, bool circle) {
    int half = brush_size / 2;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++) {
            if (circle && dx * dx + dy * dy > half * half)
                continue;
            cs.active().set(x + dx, y + dy, color);
        }
}

static void bresenham(CanvasState& cs, int x0, int y0, int x1, int y1, uint32_t color, int brush_size, bool circle) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        paint_pixel(cs, x0, y0, color, brush_size, circle);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void flood_fill(CanvasState& cs, int sx, int sy, uint32_t new_col) {
    uint32_t old_col = cs.active().get(sx, sy);
    if (old_col == new_col)
        return;
    std::queue<std::pair<int, int>> q;
    q.push({sx, sy});
    cs.active().set(sx, sy, new_col);
    const int ddx[] = {1, -1, 0, 0}, ddy[] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        for (int i = 0; i < 4; i++) {
            int nx = x + ddx[i], ny = y + ddy[i];
            if (cs.active().in_bounds(nx, ny) && cs.active().get(nx, ny) == old_col) {
                cs.active().set(nx, ny, new_col);
                q.push({nx, ny});
            }
        }
    }
}

static void draw_rect(CanvasState& cs, int x0, int y0, int x1, int y1, uint32_t color, bool filled) {
    int minx = std::min(x0, x1), maxx = std::max(x0, x1);
    int miny = std::min(y0, y1), maxy = std::max(y0, y1);
    if (filled) {
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++)
                cs.active().set(x, y, color);
    } else {
        for (int x = minx; x <= maxx; x++) {
            cs.active().set(x, miny, color);
            cs.active().set(x, maxy, color);
        }
        for (int y = miny + 1; y < maxy; y++) {
            cs.active().set(minx, y, color);
            cs.active().set(maxx, y, color);
        }
    }
}

static void draw_ellipse(CanvasState& cs, int x0, int y0, int x1, int y1, uint32_t color, bool filled) {
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int rx = std::abs(x1 - x0) / 2, ry = std::abs(y1 - y0) / 2;
    if (rx == 0 && ry == 0) {
        cs.active().set(cx, cy, color);
        return;
    }
    if (filled) {
        for (int y = -ry; y <= ry; y++) {
            float t = (rx > 0 && ry > 0) ? (float)(rx * rx) * (1.0f - (float)(y * y) / (float)(ry * ry)) : 0.0f;
            int xw  = (t > 0.0f) ? (int)std::sqrt(t) : 0;
            for (int x = -xw; x <= xw; x++)
                cs.active().set(cx + x, cy + y, color);
        }
    } else {
        // Dual scan: row-by-row then column-by-column — guarantees gap-free outline
        for (int dy = -ry; dy <= ry; dy++) {
            float t = (ry > 0) ? (float)(rx * rx) * (1.0f - (float)(dy * dy) / (float)(ry * ry)) : 0.0f;
            int xw  = (t > 0.0f) ? (int)std::sqrt(t) : 0;
            cs.active().set(cx - xw, cy + dy, color);
            cs.active().set(cx + xw, cy + dy, color);
        }
        for (int dx = -rx; dx <= rx; dx++) {
            float t = (rx > 0) ? (float)(ry * ry) * (1.0f - (float)(dx * dx) / (float)(rx * rx)) : 0.0f;
            int yw  = (t > 0.0f) ? (int)std::sqrt(t) : 0;
            cs.active().set(cx + dx, cy - yw, color);
            cs.active().set(cx + dx, cy + yw, color);
        }
    }
}

void panels::DrawCanvas(CanvasState& cs, const ToolsState& tools, const PaletteState& palette, SelectionState& sel) {
    static GLuint texture = 0;
    static int tex_w = 0, tex_h = 0;
    static ImVec2 last_px      = {-1.0f, -1.0f};
    static bool was_painting   = false;
    static bool shape_dragging = false;
    static int shape_sx = -1, shape_sy = -1;

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
    ImGuiIO& io = ImGui::GetIO();

    // Center canvas in the window on first render after creation
    if (cs.needs_center) {
        ImVec2 avail    = ImGui::GetContentRegionAvail();
        float W         = cs.width() * cs.zoom;
        float H         = cs.height() * cs.zoom;
        cs.pan          = {(avail.x - W) * 0.5f, (avail.y - H) * 0.5f};
        cs.needs_center = false;
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

    // Tool input — compute canvas coords early so shape commit can use them outside hovered block
    ImVec2 cur_origin = {base.x + cs.pan.x, base.y + cs.pan.y};
    int px            = (int)((io.MousePos.x - cur_origin.x) / cs.zoom);
    int py            = (int)((io.MousePos.y - cur_origin.y) / cs.zoom);

    // Shape preview overlay — always draw while dragging, regardless of hover state
    if (shape_dragging) {
        float ssx      = cur_origin.x + shape_sx * cs.zoom;
        float ssy      = cur_origin.y + shape_sy * cs.zoom;
        float sex      = cur_origin.x + (px + 1) * cs.zoom;
        float sey      = cur_origin.y + (py + 1) * cs.zoom;
        ImU32 prev_col = (ImGui::ColorConvertFloat4ToU32(palette.primary_color) & 0x00FFFFFFu) | 0xCC000000u;
        // Clamp preview coords to screen for clarity
        ImVec2 p1 = {ssx, ssy}, p2 = {sex, sey};
        int t = tools.active_tool;
        if (t == 3) {
            dl->AddLine(p1, {cur_origin.x + px * cs.zoom, cur_origin.y + py * cs.zoom}, prev_col, 1.5f);
        } else if (t == 4 || t == 5) {
            if (t == 5)
                dl->AddRectFilled(p1, p2, prev_col);
            else
                dl->AddRect(p1, p2, prev_col, 0.0f, 0, 1.5f);
        } else if (t == 6 || t == 7) {
            float ecx = (p1.x + p2.x) * 0.5f, ecy = (p1.y + p2.y) * 0.5f;
            float erx = std::abs(p2.x - p1.x) * 0.5f, ery = std::abs(p2.y - p1.y) * 0.5f;
            if (t == 7)
                dl->AddEllipseFilled({ecx, ecy}, {erx, ery}, prev_col);
            else
                dl->AddEllipse({ecx, ecy}, {erx, ery}, prev_col, 0.0f, 0, 1.5f);
        } else if (t == 9) {
            dl->AddRect(p1, p2, IM_COL32(255,255,255,200), 0, 0, 1.5f);
            dl->AddRect({p1.x+1,p1.y+1},{p2.x-1,p2.y-1}, IM_COL32(0,0,0,200), 0, 0, 1.0f);
        }
    }

    // Move tool — left-drag pans just like middle-mouse
    if (tools.active_tool == 8 && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
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

        uint32_t color = (tools.active_tool == 1) ? 0x00000000u : ImGui::ColorConvertFloat4ToU32(palette.primary_color);

        if (tools.active_tool == 2) {
            was_painting = false;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                cs.push_snapshot();
                Log("Flood fill at (%d,%d) color #%08X", px, py, color);
                flood_fill(cs, px, py, color);
                cs.rebuild_composite();
            }
        } else if (tools.active_tool >= 3 && tools.active_tool <= 7) {
            // Shape tools — start drag on click
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
            if (!shape_dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                cs.push_snapshot();
                shape_sx       = px;
                shape_sy       = py;
                shape_dragging = true;
                Log("Shape start at (%d,%d) tool=%d", px, py, tools.active_tool);
            }
        } else if (tools.active_tool == 8) {
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
        } else if (tools.active_tool == 9) {
            was_painting = false;
            last_px      = {-1.0f, -1.0f};
            if (!shape_dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                shape_sx       = px;
                shape_sy       = py;
                shape_dragging = true;
            }
        } else {
            bool is_painting = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            if (is_painting && !was_painting) {
                cs.push_snapshot();
                Log("Stroke start at (%d,%d) tool=%d size=%d", px, py, tools.active_tool, tools.brush_size);
            }
            if (is_painting) {
                if (last_px.x < 0.0f)
                    paint_pixel(cs, px, py, color, tools.brush_size, tools.circle_brush);
                else
                    bresenham(cs, (int)last_px.x, (int)last_px.y, px, py, color, tools.brush_size, tools.circle_brush);
                last_px = {(float)px, (float)py};
                cs.rebuild_composite();
            } else {
                last_px = {-1.0f, -1.0f};
            }
            was_painting = is_painting;
        }
    } else {
        // Mouse left the canvas — reset brush state but keep shape drag alive
        last_px      = {-1.0f, -1.0f};
        was_painting = false;
    }

    // Commit shape on mouse release regardless of hover state
    if (shape_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Log("Shape commit tool=%d (%d,%d)->(%d,%d)", tools.active_tool, shape_sx, shape_sy, px, py);
        if (tools.active_tool == 9) {
            int cw = cs.width(), ch = cs.height();
            int ax = std::clamp(std::min(shape_sx, px), 0, cw - 1);
            int ay = std::clamp(std::min(shape_sy, py), 0, ch - 1);
            int bx = std::clamp(std::max(shape_sx, px), 0, cw - 1);
            int by = std::clamp(std::max(shape_sy, py), 0, ch - 1);
            sel.active = (ax != bx || ay != by);
            if (sel.active) { sel.x0 = ax; sel.y0 = ay; sel.x1 = bx; sel.y1 = by; }
        } else {
            uint32_t color = ImGui::ColorConvertFloat4ToU32(palette.primary_color);
            switch (tools.active_tool) {
            case 3: bresenham(cs, shape_sx, shape_sy, px, py, color, tools.brush_size, tools.circle_brush); break;
            case 4: draw_rect(cs, shape_sx, shape_sy, px, py, color, false); break;
            case 5: draw_rect(cs, shape_sx, shape_sy, px, py, color, true);  break;
            case 6: draw_ellipse(cs, shape_sx, shape_sy, px, py, color, false); break;
            case 7: draw_ellipse(cs, shape_sx, shape_sy, px, py, color, true);  break;
            }
            cs.rebuild_composite();
        }
        shape_dragging = false;
    }

    ImGui::End();
}
