#include "canvas_panel.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cstdlib>

static void paint_pixel(CanvasState& cs, int x, int y, uint32_t color, int brush_size) {
    int half = brush_size / 2;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++)
            cs.canvas.set(x + dx, y + dy, color);
    cs.dirty = true;
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

void panels::DrawCanvas(CanvasState& cs, const ToolsState& tools, const PaletteState& palette) {
    static GLuint  texture = 0;
    static int     tex_w   = 0, tex_h = 0;
    static ImVec2  last_px = { -1.0f, -1.0f };

    // (Re)create texture on first call or if canvas dimensions changed
    if (texture == 0 || tex_w != cs.canvas.width || tex_h != cs.canvas.height) {
        if (texture != 0) glDeleteTextures(1, &texture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.canvas.width, cs.canvas.height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, cs.canvas.pixels.data());
        tex_w    = cs.canvas.width;
        tex_h    = cs.canvas.height;
        cs.dirty = false;
    } else if (cs.dirty) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.canvas.width, cs.canvas.height,
                        GL_RGBA, GL_UNSIGNED_BYTE, cs.canvas.pixels.data());
        cs.dirty = false;
    }

    ImGui::Begin("Canvas");
    ImGuiIO& io = ImGui::GetIO();

    // Pan with middle-mouse drag
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        cs.pan.x += io.MouseDelta.x;
        cs.pan.y += io.MouseDelta.y;
    }

    // ImGui v1.92+ binds a linear sampler that overrides texture parameters — switch to nearest.
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    ImDrawList*      dl  = ImGui::GetWindowDrawList();
    if (pio.DrawCallback_SetSamplerNearest)
        dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);

    // Apply pan offset before drawing
    ImVec2 base     = ImGui::GetCursorScreenPos();
    ImVec2 origin   = { base.x + cs.pan.x, base.y + cs.pan.y };
    ImGui::SetCursorScreenPos(origin);

    ImVec2 display_size = { cs.canvas.width * cs.zoom, cs.canvas.height * cs.zoom };
    ImGui::Image((ImTextureID)(uintptr_t)texture, display_size);

    if (pio.DrawCallback_SetSamplerLinear)
        dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);

    // Pixel grid overlay (zoom >= 4x only)
    if (cs.zoom >= 4.0f) {
        ImU32  gcol = IM_COL32(80, 80, 80, 100);
        float  W    = cs.canvas.width  * cs.zoom;
        float  H    = cs.canvas.height * cs.zoom;
        for (int x = 0; x <= cs.canvas.width; x++) {
            float sx = origin.x + x * cs.zoom;
            dl->AddLine({ sx, origin.y }, { sx, origin.y + H }, gcol);
        }
        for (int y = 0; y <= cs.canvas.height; y++) {
            float sy = origin.y + y * cs.zoom;
            dl->AddLine({ origin.x, sy }, { origin.x + W, sy }, gcol);
        }
    }

    // Brush / eraser input
    if (ImGui::IsItemHovered()) {
        if (io.MouseWheel != 0.0f)
            cs.zoom = std::clamp(cs.zoom + io.MouseWheel, 1.0f, 32.0f);

        int px = (int)((io.MousePos.x - origin.x) / cs.zoom);
        int py = (int)((io.MousePos.y - origin.y) / cs.zoom);

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            uint32_t color = (tools.active_tool == 1)
                ? 0xFFFFFFFFu
                : ImGui::ColorConvertFloat4ToU32(palette.primary_color);

            if (last_px.x < 0.0f)
                paint_pixel(cs, px, py, color, tools.brush_size);
            else
                bresenham(cs, (int)last_px.x, (int)last_px.y, px, py, color, tools.brush_size);
            last_px = { (float)px, (float)py };
        } else {
            last_px = { -1.0f, -1.0f };
        }
    } else {
        last_px = { -1.0f, -1.0f };
    }

    ImGui::End();
}
