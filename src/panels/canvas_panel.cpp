#include "canvas_panel.h"
#include "canvas_panel_internal.h"
#include "canvas_overlay.h"
#include "canvas_tools.h"
#include "pixc_io.h"
#include "log.h"
#include "imgui.h"
#include "ui_scale.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

static std::vector<DocRenderState> s_doc_render;
static std::vector<uint32_t>       s_onion_buf;        // shared temp buf for onion skin upload
static int                         s_tabclose_doc_idx = -1; // persists while unsaved-changes modal is open

static void close_doc(AppState& app, int idx) {
    if (idx < 0 || idx >= (int)app.docs.size()) return;
    // Free GL textures
    if (idx < (int)s_doc_render.size()) {
        DocRenderState& drs = s_doc_render[idx];
        if (drs.texture)      glDeleteTextures(1, &drs.texture);
        if (drs.onion_tex[0]) glDeleteTextures(1, &drs.onion_tex[0]);
        if (drs.onion_tex[1]) glDeleteTextures(1, &drs.onion_tex[1]);
        s_doc_render.erase(s_doc_render.begin() + idx);
    }
    app.docs.erase(app.docs.begin() + idx);
    // Always keep at least one document
    if (app.docs.empty()) {
        app.docs.emplace_back();
        s_doc_render.emplace_back();
    }
    app.active_doc = std::min(app.active_doc, (int)app.docs.size() - 1);
}


void panels::DrawCanvas(AppState& app) {
    // Grow per-doc render state to match document count
    while (s_doc_render.size() < app.docs.size()) s_doc_render.emplace_back();

    ImGui::Begin("Canvas");

    // --- Consume close signals from Ctrl+W / File>Close and tab X click ---
    {
        int close_idx = -1;
        if (app.close_active_doc_requested) {
            app.close_active_doc_requested = false;
            close_idx = app.active_doc;
        }
        if (app.close_doc_idx_requested >= 0) {
            close_idx = app.close_doc_idx_requested;
            app.close_doc_idx_requested = -1;
        }
        if (close_idx >= 0 && close_idx < (int)app.docs.size()) {
            if (app.docs[close_idx].canvas.unsaved_changes) {
                s_tabclose_doc_idx = close_idx;
                ImGui::OpenPopup("Unsaved Changes##tabclose");
            } else {
                close_doc(app, close_idx);
            }
        }
    }

    if (ImGui::BeginPopupModal("Unsaved Changes##tabclose", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save changes before closing?");
        ImGui::Spacing();
        if (ImGui::Button("Save")) {
            if (s_tabclose_doc_idx >= 0 && s_tabclose_doc_idx < (int)app.docs.size()) {
                auto& d = app.docs[s_tabclose_doc_idx];
                if (!d.project_path.empty()) {
                    int prev = app.active_doc;
                    app.active_doc = s_tabclose_doc_idx;
                    if (pixc_io::save(app, d.project_path))
                        app.canvas().unsaved_changes = false;
                    app.active_doc = prev;
                }
                close_doc(app, s_tabclose_doc_idx);
            }
            s_tabclose_doc_idx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save")) {
            if (s_tabclose_doc_idx >= 0 && s_tabclose_doc_idx < (int)app.docs.size())
                close_doc(app, s_tabclose_doc_idx);
            s_tabclose_doc_idx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            s_tabclose_doc_idx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Bind state for the active document ---
    CanvasState&    cs      = app.canvas();
    ToolsState&     tools   = app.tools;
    PaletteState&   palette = app.palette();
    SelectionState& sel     = app.selection();
    DocRenderState& drs     = s_doc_render[app.active_doc];

    // --- (Re)create texture on first call or if canvas dimensions changed ---
    if (drs.texture == 0 || drs.tex_w != cs.width() || drs.tex_h != cs.height()) {
        if (drs.texture != 0)
            glDeleteTextures(1, &drs.texture);
        if (drs.onion_tex[0] != 0)
            glDeleteTextures(2, drs.onion_tex);
        glGenTextures(1, &drs.texture);
        glBindTexture(GL_TEXTURE_2D, drs.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.width(), cs.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     cs.composite.data());
        glGenTextures(2, drs.onion_tex);
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, drs.onion_tex[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cs.width(), cs.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        drs.tex_w = cs.width();
        drs.tex_h = cs.height();
        cs.dirty  = false;
    } else if (cs.dirty) {
        glBindTexture(GL_TEXTURE_2D, drs.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                        cs.composite.data());
        cs.dirty = false;
    }

    if (tools.onion_skin) {
        bool show_prev = tools.onion_skin_mode != 2 && cs.active_frame > 0;
        bool show_next = tools.onion_skin_mode != 1 && cs.active_frame < (int)cs.frames.size() - 1;
        if (show_prev) {
            cs.composite_frame(cs.active_frame - 1, s_onion_buf);
            glBindTexture(GL_TEXTURE_2D, drs.onion_tex[0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE, s_onion_buf.data());
        }
        if (show_next) {
            cs.composite_frame(cs.active_frame + 1, s_onion_buf);
            glBindTexture(GL_TEXTURE_2D, drs.onion_tex[1]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cs.width(), cs.height(), GL_RGBA, GL_UNSIGNED_BYTE, s_onion_buf.data());
        }
    }

    // Commit floating selection automatically when tool changes away from rect select
    if (sel.floating && tools.active_tool != tool::RectSelect) {
        commit_floating(cs, sel);
        drs.drag.handle_dragging = false;
        drs.drag.active_handle   = -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool any_popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

    // Center canvas once the docking layout has settled.
    // On first launch (no imgui.ini), the panel resizes over several frames as docking applies.
    // We compare avail against the previous frame's value; only center when both are stable.
    if (cs.needs_center) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 0 && avail.y > 0) {
            if (avail.x == drs.prev_avail.x && avail.y == drs.prev_avail.y) {
                float W         = cs.width() * cs.zoom;
                float H         = cs.height() * cs.zoom;
                cs.pan          = {(avail.x - W) * 0.5f, (avail.y - H) * 0.5f};
                cs.needs_center = false;
            }
            drs.prev_avail = avail;
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
    draw_canvas_overlays(dl, cs, tools, drs.onion_tex, origin, W, H);

    ImGui::SetCursorScreenPos(origin);
    ImGui::Image((ImTextureID)(uintptr_t)drs.texture, {W, H});

    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if (pio.DrawCallback_SetSamplerLinear)
        dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);

    draw_canvas_decorations(dl, cs, tools, sel, origin, W, H, drs.drag, hpos);

    // Tool input
    ImVec2 cur_origin  = {std::round(base.x + cs.pan.x), std::round(base.y + cs.pan.y)};
    int    px          = (int)((io.MousePos.x - cur_origin.x) / cs.zoom);
    int    py          = (int)((io.MousePos.y - cur_origin.y) / cs.zoom);

    ImVec2 wpos  = ImGui::GetWindowPos();
    ImVec2 wsize = ImGui::GetWindowSize();
    bool mouse_in_win = io.MousePos.x >= wpos.x && io.MousePos.x < wpos.x + wsize.x
                     && io.MousePos.y >= wpos.y && io.MousePos.y < wpos.y + wsize.y;

    CanvasToolInputCtx ctx{cs, tools, sel, palette, drs, dl, base, cur_origin, px, py,
                           mouse_in_win, any_popup, hpos, W, H};
    handle_canvas_tool_input(ctx);

    ImGui::End();
}
