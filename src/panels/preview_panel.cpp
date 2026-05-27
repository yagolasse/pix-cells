#include "preview_panel.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cmath>

static GLuint  s_tex   = 0;
static int     s_tex_w = 0;
static int     s_tex_h = 0;
static float   s_zoom  = 0.0f; // 0 = uninitialised; computed on first canvas load
static ImVec2  s_pan   = {0.0f, 0.0f};

// Choose a zoom that makes max(cw,ch) fill ~160px of the default 200px window.
static float fit_zoom(int cw, int ch) {
    float z = std::floor(160.0f / static_cast<float>(std::max(cw, ch)));
    return std::max(1.0f, z);
}

void panels::DrawPreview(const CanvasState& cs, bool* p_open) {
    if (!p_open || !*p_open)
        return;

    int cw = cs.width();
    int ch = cs.height();

    if (s_tex == 0 || s_tex_w != cw || s_tex_h != ch) {
        if (s_tex != 0)
            glDeleteTextures(1, &s_tex);
        glGenTextures(1, &s_tex);
        glBindTexture(GL_TEXTURE_2D, s_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     cs.composite.data());
        s_tex_w = cw;
        s_tex_h = ch;
        s_pan   = {0.0f, 0.0f};
        s_zoom  = fit_zoom(cw, ch);
    } else {
        glBindTexture(GL_TEXTURE_2D, s_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch, GL_RGBA, GL_UNSIGNED_BYTE,
                        cs.composite.data());
    }

    ImGuiIO& io = ImGui::GetIO();
    // "##pixcells" suffix gives a fresh ini key (avoids stale size from earlier
    // AlwaysAutoResize builds) while the displayed title stays "Preview".
    ImGui::SetNextWindowPos({io.DisplaySize.x - 220.0f, 40.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({200.0f, 200.0f}, ImGuiCond_FirstUseEver);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin("Preview##pixcells", p_open, flags)) {
        ImGui::End();
        return;
    }

    ImVec2 content_min   = ImGui::GetCursorScreenPos();
    ImVec2 content_avail = ImGui::GetContentRegionAvail();
    ImVec2 clip_max      = {content_min.x + content_avail.x, content_min.y + content_avail.y};

    if (ImGui::IsWindowHovered()) {
        // Pan: middle-mouse drag (all platforms) or right-mouse drag (Mac-friendly).
        bool panning = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
                       ImGui::IsMouseDragging(ImGuiMouseButton_Right,  0.0f);
        if (panning) {
            s_pan.x += io.MouseDelta.x;
            s_pan.y += io.MouseDelta.y;
        }

        // Scroll to zoom with integer steps (matches canvas_panel) so W = cw * zoom
        // is always a whole number of pixels — prevents sub-pixel edge bleed.
        float wheel = io.MouseWheel;
        if (wheel != 0.0f) {
            float old_zoom = s_zoom;
            float dir      = wheel > 0.0f ? 1.0f : -1.0f;
            float new_zoom;
            if (dir > 0.0f && s_zoom >= 8.0f)
                new_zoom = std::ceil((s_zoom  + 1.0f) / 4.0f) * 4.0f;
            else if (dir < 0.0f && s_zoom > 8.0f)
                new_zoom = std::floor((s_zoom - 1.0f) / 4.0f) * 4.0f;
            else
                new_zoom = s_zoom + dir;
            new_zoom = std::clamp(new_zoom, 1.0f, 32.0f);
            if (new_zoom != old_zoom) {
                ImVec2 rel = {io.MousePos.x - content_min.x, io.MousePos.y - content_min.y};
                s_pan.x = rel.x - (rel.x - s_pan.x) / old_zoom * new_zoom;
                s_pan.y = rel.y - (rel.y - s_pan.y) / old_zoom * new_zoom;
                s_zoom  = new_zoom;
            }
        }
    }

    float  W      = static_cast<float>(cw) * s_zoom;
    float  H      = static_cast<float>(ch) * s_zoom;
    ImVec2 origin = {std::round(content_min.x + s_pan.x), std::round(content_min.y + s_pan.y)};

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Checkerboard background (clipped to image bounds, matching canvas_panel behaviour)
    {
        float cell = cs.checker_size * s_zoom;
        ImU32 col1 = ImGui::ColorConvertFloat4ToU32(cs.checker_color1);
        ImU32 col2 = ImGui::ColorConvertFloat4ToU32(cs.checker_color2);
        dl->PushClipRect(origin, {origin.x + W, origin.y + H}, true);
        int rows = static_cast<int>(H / cell) + 1;
        int cols = static_cast<int>(W / cell) + 1;
        for (int row = 0; row < rows; row++)
            for (int col = 0; col < cols; col++) {
                float cx = origin.x + static_cast<float>(col) * cell;
                float cy = origin.y + static_cast<float>(row) * cell;
                dl->AddRectFilled({cx, cy}, {cx + cell, cy + cell},
                                  (row + col) % 2 ? col2 : col1);
            }
        dl->PopClipRect();
    }

    // Image (clipped to content area, GL_NEAREST sampler for pixel art)
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    dl->PushClipRect(content_min, clip_max, true);
    if (pio.DrawCallback_SetSamplerNearest)
        dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);
    dl->AddImage((ImTextureID)(uintptr_t)s_tex, origin, {origin.x + W, origin.y + H});
    if (pio.DrawCallback_SetSamplerLinear)
        dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);
    dl->PopClipRect();

    // Occupy the content area so the window has a stable layout.
    ImGui::Dummy(content_avail);

    ImGui::End();
}
