#include "canvas_panel.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cstdlib>
#include <queue>

static void paint_pixel(CanvasState& cs, int x, int y, uint32_t color, int brush_size) {
    int half = brush_size / 2;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++)
            cs.active().set(x + dx, y + dy, color);
}

static void bresenham(CanvasState& cs, int x0, int y0, int x1, int y1,
                      uint32_t color, int brush_size) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        paint_pixel(cs, x0, y0, color, brush_size);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void flood_fill(CanvasState& cs, int sx, int sy, uint32_t new_col) {
    uint32_t old_col = cs.active().get(sx, sy);
    if (old_col == new_col) return;
    std::queue<std::pair<int,int>> q;
    q.push({sx, sy});
    cs.active().set(sx, sy, new_col);
    const int ddx[] = {1,-1,0,0}, ddy[] = {0,0,1,-1};
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();
        for (int i = 0; i < 4; i++) {
            int nx = x+ddx[i], ny = y+ddy[i];
            if (cs.active().in_bounds(nx,ny) && cs.active().get(nx,ny) == old_col) {
                cs.active().set(nx, ny, new_col);
                q.push({nx, ny});
            }
        }
    }
}

void panels::DrawCanvas(CanvasState& cs, const ToolsState& tools, const PaletteState& palette) {
    static GLuint  texture     = 0;
    static int     tex_w       = 0, tex_h = 0;
    static ImVec2  last_px     = { -1.0f, -1.0f };
    static bool    was_painting = false;

    // (Re)create texture on first call or if canvas dimensions changed
    if (texture == 0 || tex_w != cs.width() || tex_h != cs.height()) {
        if (texture != 0) glDeleteTextures(1, &texture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.width(), cs.height(),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, cs.composite.data());
        tex_w    = cs.width();
        tex_h    = cs.height();
        cs.dirty = false;
    } else if (cs.dirty) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(),
                        GL_RGBA, GL_UNSIGNED_BYTE, cs.composite.data());
        cs.dirty = false;
    }

    ImGui::Begin("Canvas");
    ImGuiIO& io = ImGui::GetIO();

    // Pan with middle-mouse drag
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        cs.pan.x += io.MouseDelta.x;
        cs.pan.y += io.MouseDelta.y;
    }

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      base   = ImGui::GetCursorScreenPos();
    ImVec2      origin = { base.x + cs.pan.x, base.y + cs.pan.y };
    float       W      = cs.width()  * cs.zoom;
    float       H      = cs.height() * cs.zoom;

    // Checkerboard background — visible through transparent pixels
    {
        float cell = 8.0f;
        for (float cy = origin.y; cy < origin.y + H; cy += cell)
            for (float cx = origin.x; cx < origin.x + W; cx += cell) {
                int   parity = (int)((cy - origin.y) / cell + (cx - origin.x) / cell) % 2;
                ImU32 col    = parity ? IM_COL32(170,170,170,255) : IM_COL32(220,220,220,255);
                dl->AddRectFilled({cx, cy}, {std::min(cx+cell, origin.x+W), std::min(cy+cell, origin.y+H)}, col);
            }
    }

    // ImGui v1.92+ binds a linear sampler that overrides texture parameters — switch to nearest.
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if (pio.DrawCallback_SetSamplerNearest)
        dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);

    ImGui::SetCursorScreenPos(origin);
    ImGui::Image((ImTextureID)(uintptr_t)texture, { W, H });

    if (pio.DrawCallback_SetSamplerLinear)
        dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);

    // Pixel grid overlay (zoom >= 4x only)
    if (cs.zoom >= 4.0f) {
        ImU32 gcol = IM_COL32(80, 80, 80, 100);
        for (int x = 0; x <= cs.width(); x++) {
            float sx = origin.x + x * cs.zoom;
            dl->AddLine({ sx, origin.y }, { sx, origin.y + H }, gcol);
        }
        for (int y = 0; y <= cs.height(); y++) {
            float sy = origin.y + y * cs.zoom;
            dl->AddLine({ origin.x, sy }, { origin.x + W, sy }, gcol);
        }
    }

    // Tool input
    if (ImGui::IsItemHovered()) {
        if (io.MouseWheel != 0.0f)
            cs.zoom = std::clamp(cs.zoom + io.MouseWheel, 1.0f, 32.0f);

        int px = (int)((io.MousePos.x - origin.x) / cs.zoom);
        int py = (int)((io.MousePos.y - origin.y) / cs.zoom);

        uint32_t color = (tools.active_tool == 1)
            ? 0x00000000u
            : ImGui::ColorConvertFloat4ToU32(palette.primary_color);

        if (tools.active_tool == 2) {
            was_painting = false;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                cs.push_snapshot();
                flood_fill(cs, px, py, color);
                cs.rebuild_composite();
            }
        } else {
            bool is_painting = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            if (is_painting && !was_painting) cs.push_snapshot();
            if (is_painting) {
                if (last_px.x < 0.0f)
                    paint_pixel(cs, px, py, color, tools.brush_size);
                else
                    bresenham(cs, (int)last_px.x, (int)last_px.y, px, py, color, tools.brush_size);
                last_px = { (float)px, (float)py };
                cs.rebuild_composite();
            } else {
                last_px = { -1.0f, -1.0f };
            }
            was_painting = is_painting;
        }
    } else {
        last_px      = { -1.0f, -1.0f };
        was_painting = false;
    }

    ImGui::End();
}
