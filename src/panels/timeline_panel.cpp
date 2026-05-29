#include "timeline_panel.h"
#include "app_state.h"
#include "log.h"
#include "imgui.h"
#include "icon_manager.h"
#include "ui_scale.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static bool   s_playing       = false;
static double s_next_frame_at = 0.0;

// Per-frame preview textures (RGBA8, sized to canvas). Only recomposited when a
// frame's revision counter changes (tracked via CanvasState::frame_revisions).
static std::vector<GLuint>        s_thumbs;
static std::vector<uint64_t>      s_seen_revisions;
static int                        s_thumb_w = 0, s_thumb_h = 0;
static const CanvasState*         s_last_cs = nullptr; // detects document switches

// Returns true if this frame card was clicked
static bool draw_frame_card(int idx, bool is_active, bool in_range,
                            GLuint tex, int cw, int ch) {
    float card_w = ui_scale::px(56.0f);
    float card_h = ui_scale::px(64.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    ImU32 bg = is_active
        ? IM_COL32(80, 130, 200, 220)
        : IM_COL32(50, 50, 60, 200);
    dl->AddRectFilled(pos, { pos.x + card_w, pos.y + card_h }, bg, 4.0f);

    // Thumbnail (aspect-fit within padded card area)
    if (tex != 0 && cw > 0 && ch > 0) {
        const float pad = 5.0f;
        float avail_w = card_w - 2 * pad;
        float avail_h = card_h - 2 * pad;
        float scale = std::min(avail_w / (float)cw, avail_h / (float)ch);
        float tw = static_cast<float>(cw) * scale, th = static_cast<float>(ch) * scale;
        ImVec2 tp0 = { pos.x + (card_w - tw) * 0.5f, pos.y + (card_h - th) * 0.5f };
        ImVec2 tp1 = { tp0.x + tw, tp0.y + th };
        ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        if (pio.DrawCallback_SetSamplerNearest)
            dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);
        dl->AddImage(tex, tp0, tp1);
        if (pio.DrawCallback_SetSamplerLinear)
            dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);
    }

    // Dim cards that fall outside the active tag's playback range
    if (!in_range)
        dl->AddRectFilled(pos, { pos.x + card_w, pos.y + card_h }, IM_COL32(20, 20, 25, 140), 4.0f);

    // Border
    ImU32 border = is_active
        ? IM_COL32(120, 180, 255, 255)
        : IM_COL32(80, 80, 100, 200);
    dl->AddRect(pos, { pos.x + card_w, pos.y + card_h }, border, 4.0f, 0, 1.5f);

    // Frame-number badge (top-left, over the art)
    char label[16];
    snprintf(label, sizeof(label), "%d", idx + 1);
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 b0 = { pos.x + 2.0f, pos.y + 2.0f };
    ImVec2 b1 = { b0.x + ts.x + 6.0f, b0.y + ts.y + 2.0f };
    dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 150), 3.0f);
    dl->AddText({ b0.x + 3.0f, b0.y + 1.0f }, IM_COL32(230, 230, 230, 255), label);

    // Invisible button for interaction
    ImGui::InvisibleButton("##card", { card_w, card_h });
    return ImGui::IsItemClicked();
}

static void draw_add_card() {
    float card_w = ui_scale::px(40.0f);
    float card_h = ui_scale::px(64.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, { pos.x + card_w, pos.y + card_h }, IM_COL32(40, 40, 50, 180), 4.0f);
    dl->AddRect(pos, { pos.x + card_w, pos.y + card_h }, IM_COL32(80, 80, 100, 180), 4.0f, 0, 1.5f);

    // "+" text centered
    const char* plus = "+";
    ImVec2 plus_size = ImGui::CalcTextSize(plus);
    dl->AddText(
        { pos.x + (card_w - plus_size.x) * 0.5f, pos.y + (card_h - plus_size.y) * 0.5f },
        IM_COL32(160, 200, 160, 255),
        plus
    );

    ImGui::InvisibleButton("##add_card", { card_w, card_h });
}

// Refresh per-frame preview textures. Only recomposites frames whose revision
// counter changed since the last upload.
static void update_thumbnails(CanvasState& cs) {
    int cw = cs.width(), ch = cs.height();
    if (cw <= 0 || ch <= 0) return;

    // Document switch: force all thumbnails to rebuild
    if (&cs != s_last_cs) {
        s_seen_revisions.clear();
        s_last_cs = &cs;
    }

    if (s_thumb_w != cw || s_thumb_h != ch) {
        if (!s_thumbs.empty())
            glDeleteTextures((GLsizei)s_thumbs.size(), s_thumbs.data());
        s_thumbs.clear();
        s_seen_revisions.clear(); // force full rebuild after resize
        s_thumb_w = cw;
        s_thumb_h = ch;
    }

    int nframes = (int)cs.frames.size();
    if ((int)s_thumbs.size() < nframes) {
        size_t old = s_thumbs.size();
        s_thumbs.resize(nframes, 0);
        s_seen_revisions.resize(nframes, 0); // 0 never matches a frame_revisions entry
        glGenTextures((GLsizei)(nframes - old), s_thumbs.data() + old);
        for (size_t i = old; i < s_thumbs.size(); i++) {
            glBindTexture(GL_TEXTURE_2D, s_thumbs[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
    } else if ((int)s_thumbs.size() > nframes) {
        glDeleteTextures((GLsizei)(s_thumbs.size() - nframes), s_thumbs.data() + nframes);
        s_thumbs.resize(nframes);
        s_seen_revisions.resize(nframes);
    }

    static std::vector<uint32_t> buf;
    for (int i = 0; i < nframes; i++) {
        uint64_t cur = (i < (int)cs.frame_revisions.size()) ? cs.frame_revisions[i] : 0;
        if (i < (int)s_seen_revisions.size() && s_seen_revisions[i] == cur) continue;
        cs.composite_frame(i, buf);
        glBindTexture(GL_TEXTURE_2D, s_thumbs[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        if (i < (int)s_seen_revisions.size())
            s_seen_revisions[i] = cur;
    }
}

void panels::DrawTimeline(CanvasState& cs) {
    // Clamp active_frame to valid range
    if (!cs.frames.empty() && cs.active_frame >= (int)cs.frames.size())
        cs.active_frame = (int)cs.frames.size() - 1;

    // Active playback range (a tag's span, or all frames)
    int lo = 0, hi = (int)cs.frames.size() - 1;
    if (cs.active_tag >= 0 && cs.active_tag < (int)cs.tags.size()) {
        const AnimTag& t = cs.tags[cs.active_tag];
        lo = std::clamp(t.start, 0, hi);
        hi = std::clamp(t.end, lo, hi);
    }
    int span = hi - lo + 1;

    // Advance playback
    if (s_playing) {
        if (span <= 1) {
            s_playing = false;
        } else {
            double now = ImGui::GetTime();
            if (now >= s_next_frame_at) {
                if (cs.active_frame < lo || cs.active_frame > hi)
                    cs.active_frame = lo;
                else
                    cs.active_frame = lo + ((cs.active_frame - lo + 1) % span);
                cs.rebuild_composite();
                s_next_frame_at = now + 1.0 / (double)cs.fps;
            }
        }
    }

    ImGui::Begin("Timeline");

    update_thumbnails(cs);

    // Transport controls
    const float TBTN = ui_scale::px(22.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if (ImGui::ImageButton("##prev", icon_manager::get("skip_previous"), {TBTN, TBTN})) {
        s_playing = false;
        if (cs.active_frame < lo || cs.active_frame > hi) cs.active_frame = lo;
        else cs.active_frame = lo + ((cs.active_frame - lo - 1 + span) % span);
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Previous frame");
    ImGui::SameLine(0, 4);

    {
        const char* play_icon = s_playing ? "pause" : "play";
        if (ImGui::ImageButton("##play", icon_manager::get(play_icon), {TBTN, TBTN})) {
            s_playing = !s_playing;
            if (s_playing) {
                if (span <= 1)
                    s_playing = false;
                else
                    s_next_frame_at = ImGui::GetTime() + 1.0 / (double)cs.fps;
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(s_playing ? "Pause" : "Play");
    }
    ImGui::SameLine(0, 4);

    if (ImGui::ImageButton("##next", icon_manager::get("skip_next"), {TBTN, TBTN})) {
        s_playing = false;
        if (cs.active_frame < lo || cs.active_frame > hi) cs.active_frame = lo;
        else cs.active_frame = lo + ((cs.active_frame - lo + 1) % span);
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Next frame");

    ImGui::PopStyleVar();

    ImGui::SameLine(0, 8);

    // Editable FPS + frame count
    int fps_i = (int)cs.fps;
    ImGui::SetNextItemWidth(ui_scale::px(70.0f));
    if (ImGui::DragInt("FPS", &fps_i, 0.2f, 1, 60)) {
        cs.fps = (float)std::clamp(fps_i, 1, 60);
        Log("FPS: %d", fps_i);
    }
    ImGui::SameLine(0, 8);
    ImGui::Text("Frames: %d", (int)cs.frames.size());

    ImGui::SameLine(0, 12);

    // Active animation tag selector
    const char* tag_preview = (cs.active_tag >= 0 && cs.active_tag < (int)cs.tags.size())
        ? cs.tags[cs.active_tag].name.c_str()
        : "All frames";
    ImGui::SetNextItemWidth(ui_scale::px(120.0f));
    if (ImGui::BeginCombo("##tag", tag_preview)) {
        if (ImGui::Selectable("All frames", cs.active_tag < 0)) {
            cs.active_tag = -1;
            Log("Tag: all frames");
        }
        for (int t = 0; t < (int)cs.tags.size(); t++) {
            ImGui::PushID(t);
            if (ImGui::Selectable(cs.tags[t].name.c_str(), cs.active_tag == t)) {
                cs.active_tag = t;
                Log("Tag: \"%s\"", cs.tags[t].name.c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Tags..."))
        ImGui::OpenPopup("tags_editor");

    if (ImGui::BeginPopup("tags_editor")) {
        int last = (int)cs.frames.size() - 1;
        int del = -1;
        if (cs.tags.empty())
            ImGui::TextDisabled("No tags yet.");
        for (int t = 0; t < (int)cs.tags.size(); t++) {
            ImGui::PushID(t);
            AnimTag& tag = cs.tags[t];
            char namebuf[64];
            std::strncpy(namebuf, tag.name.c_str(), sizeof(namebuf) - 1);
            namebuf[sizeof(namebuf) - 1] = '\0';
            ImGui::SetNextItemWidth(ui_scale::px(110.0f));
            if (ImGui::InputText("##name", namebuf, sizeof(namebuf)))
                tag.name = namebuf;
            ImGui::SameLine();
            int s1 = tag.start + 1, e1 = tag.end + 1;
            ImGui::SetNextItemWidth(ui_scale::px(50.0f));
            if (ImGui::DragInt("##s", &s1, 0.1f, 1, last + 1))
                tag.start = std::clamp(s1 - 1, 0, last);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ui_scale::px(50.0f));
            if (ImGui::DragInt("##e", &e1, 0.1f, 1, last + 1))
                tag.end = std::clamp(e1 - 1, 0, last);
            if (tag.end < tag.start) tag.end = tag.start;
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) del = t;
            ImGui::PopID();
        }
        if (del >= 0) {
            Log("Tag deleted: \"%s\"", cs.tags[del].name.c_str());
            cs.tags.erase(cs.tags.begin() + del);
            if (cs.active_tag == del)      cs.active_tag = -1;
            else if (cs.active_tag > del)  cs.active_tag--;
        }
        if (ImGui::Button("Add tag")) {
            AnimTag nt;
            nt.name  = "tag";
            nt.start = 0;
            nt.end   = last;
            cs.tags.push_back(nt);
            Log("Tag added");
        }
        ImGui::EndPopup();
    }

    // Playhead slider
    ImGui::SetNextItemWidth(ui_scale::px(200.0f));
    int max_frame = std::max(0, (int)cs.frames.size() - 1);
    if (ImGui::SliderInt("##playhead", &cs.active_frame, 0, max_frame, "Frame %d")) {
        cs.rebuild_composite();
    }

    ImGui::Separator();

    // Scrollable frame card row
    ImGui::BeginChild("##frame_scroll", ImVec2(0, ui_scale::px(90.0f)), false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    int pending = 0;  // 1 = duplicate, 2 = delete
    int pending_idx = -1;
    int nframes = (int)cs.frames.size();
    int dnd_src = -1, dnd_dst = -1;

    for (int i = 0; i < nframes; i++) {
        if (i > 0) ImGui::SameLine(0.0f, 6.0f);

        ImGui::PushID(i);
        bool in_range = (i >= lo && i <= hi);
        GLuint tex = (i < (int)s_thumbs.size()) ? s_thumbs[i] : 0;
        if (draw_frame_card(i, i == cs.active_frame, in_range, tex, s_thumb_w, s_thumb_h)) {
            if (cs.active_frame != i) {
                cs.active_frame = i;
                cs.active_layer = std::min(cs.active_layer, (int)cs.frames[i].layers.size() - 1);
                cs.rebuild_composite();
                Log("Timeline: switched to frame %d", i + 1);
            }
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("FRAME_IDX", &i, sizeof(int));
            ImGui::Text("Frame %d", i + 1);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("FRAME_IDX")) {
                dnd_src = *(const int*)p->Data;
                dnd_dst = i;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##frame_ctx")) {
            if (ImGui::MenuItem("Duplicate")) { pending = 1; pending_idx = i; }
            if (ImGui::MenuItem("Delete", nullptr, false, nframes > 1)) { pending = 2; pending_idx = i; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // Add frame button
    ImGui::SameLine(0.0f, 6.0f);
    draw_add_card();
    if (ImGui::IsItemClicked()) {
        cs.push_snapshot();
        Frame f;
        Layer l;
        l.name   = "Layer 1";
        l.canvas = Canvas(cs.width(), cs.height());
        f.layers.push_back(std::move(l));
        cs.frames.push_back(std::move(f));
        cs.active_frame = (int)cs.frames.size() - 1;
        cs.active_layer = 0;
        cs.rebuild_composite();
        Log("Timeline: added frame %d", cs.active_frame + 1);
    }

    ImGui::EndChild();

    // Apply deferred frame mutation after the loop
    if (pending == 1)      cs.duplicate_frame(pending_idx);
    else if (pending == 2) cs.delete_frame(pending_idx);

    if (dnd_src >= 0 && dnd_dst >= 0 && dnd_src != dnd_dst) {
        cs.push_snapshot();
        int new_active = cs.active_frame;
        if      (cs.active_frame == dnd_src)                                                    new_active = dnd_dst;
        else if (dnd_src > dnd_dst && cs.active_frame >= dnd_dst && cs.active_frame < dnd_src) new_active++;
        else if (dnd_src < dnd_dst && cs.active_frame > dnd_src && cs.active_frame <= dnd_dst) new_active--;
        auto remap = [dnd_src, dnd_dst](int j) -> int {
            if (j == dnd_src) return dnd_dst;
            if (dnd_src > dnd_dst && j >= dnd_dst && j < dnd_src) return j + 1;
            if (dnd_src < dnd_dst && j > dnd_src && j <= dnd_dst) return j - 1;
            return j;
        };
        for (auto& tag : cs.tags) {
            tag.start = remap(tag.start);
            tag.end   = remap(tag.end);
            if (tag.start > tag.end) std::swap(tag.start, tag.end);
        }
        if (dnd_src > dnd_dst)
            std::rotate(cs.frames.begin() + dnd_dst, cs.frames.begin() + dnd_src, cs.frames.begin() + dnd_src + 1);
        else
            std::rotate(cs.frames.begin() + dnd_src, cs.frames.begin() + dnd_src + 1, cs.frames.begin() + dnd_dst + 1);
        cs.active_frame = new_active;
        cs.active_layer = std::min(cs.active_layer, (int)cs.frames[cs.active_frame].layers.size() - 1);
        cs.rebuild_composite();
        Log("Timeline: moved frame %d to position %d", dnd_src + 1, dnd_dst + 1);
    }

    ImGui::End();
}
