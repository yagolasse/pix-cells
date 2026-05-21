#include "timeline_panel.h"
#include "app_state.h"
#include "log.h"
#include "imgui.h"
#include "icon_manager.h"
#include <algorithm>
#include <cstdio>

static bool   s_playing        = false;
static double s_next_frame_at  = 0.0;

// Returns true if this frame card was clicked
static bool draw_frame_card(int idx, bool is_active, uint16_t duration_ms) {
    ImGui::PushID(idx);

    float card_w = 56.0f;
    float card_h = 64.0f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    ImU32 bg = is_active
        ? IM_COL32(80, 130, 200, 220)
        : IM_COL32(50, 50, 60, 200);
    dl->AddRectFilled(pos, { pos.x + card_w, pos.y + card_h }, bg, 4.0f);

    // Border
    ImU32 border = is_active
        ? IM_COL32(120, 180, 255, 255)
        : IM_COL32(80, 80, 100, 200);
    dl->AddRect(pos, { pos.x + card_w, pos.y + card_h }, border, 4.0f, 0, 1.5f);

    // Frame number label
    char label[16];
    snprintf(label, sizeof(label), "%d", idx + 1);
    ImVec2 text_size = ImGui::CalcTextSize(label);
    dl->AddText(
        { pos.x + (card_w - text_size.x) * 0.5f, pos.y + 8.0f },
        IM_COL32(220, 220, 220, 255),
        label
    );

    // Duration label
    char dur_label[16];
    snprintf(dur_label, sizeof(dur_label), "%dms", (int)duration_ms);
    ImVec2 dur_size = ImGui::CalcTextSize(dur_label);
    dl->AddText(
        { pos.x + (card_w - dur_size.x) * 0.5f, pos.y + card_h - 18.0f },
        IM_COL32(160, 160, 160, 255),
        dur_label
    );

    // Invisible button for interaction
    ImGui::InvisibleButton("##card", { card_w, card_h });
    bool clicked = ImGui::IsItemClicked();

    ImGui::PopID();
    return clicked;
}

static void draw_add_card() {
    float card_w = 40.0f;
    float card_h = 64.0f;

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

void panels::DrawTimeline(CanvasState& cs) {
    // Clamp active_frame to valid range
    if (!cs.frames.empty() && cs.active_frame >= (int)cs.frames.size())
        cs.active_frame = (int)cs.frames.size() - 1;

    // Advance playback
    if (s_playing) {
        if ((int)cs.frames.size() <= 1) {
            s_playing = false;
        } else {
            double now = ImGui::GetTime();
            if (now >= s_next_frame_at) {
                cs.active_frame = (cs.active_frame + 1) % (int)cs.frames.size();
                cs.rebuild_composite();
                s_next_frame_at = now + 1.0 / (double)cs.fps;
            }
        }
    }

    ImGui::Begin("Timeline");

    // Transport controls
    const float TBTN = 22.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if (ImGui::ImageButton("##prev", icon_manager::get("skip_previous"), {TBTN, TBTN})) {
        s_playing = false;
        cs.active_frame = (cs.active_frame - 1 + (int)cs.frames.size()) % (int)cs.frames.size();
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
                if ((int)cs.frames.size() <= 1)
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
        cs.active_frame = (cs.active_frame + 1) % (int)cs.frames.size();
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Next frame");

    ImGui::PopStyleVar();

    ImGui::SameLine(0, 8);

    // Status bar: FPS and frame count
    ImGui::Text("FPS: %d  |  Frames: %d", (int)cs.fps, (int)cs.frames.size());
    ImGui::SameLine();

    // Playhead slider
    ImGui::SetNextItemWidth(200.0f);
    int max_frame = std::max(0, (int)cs.frames.size() - 1);
    if (ImGui::SliderInt("##playhead", &cs.active_frame, 0, max_frame, "Frame %d")) {
        cs.rebuild_composite();
    }

    ImGui::Separator();

    // Scrollable frame card row
    ImGui::BeginChild("##frame_scroll", ImVec2(0, 90.0f), false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < (int)cs.frames.size(); i++) {
        if (i > 0) ImGui::SameLine(0.0f, 6.0f);

        uint16_t dur = cs.frames[i].duration_ms;
        if (draw_frame_card(i, i == cs.active_frame, dur)) {
            if (cs.active_frame != i) {
                cs.active_frame = i;
                cs.rebuild_composite();
                Log("Timeline: switched to frame %d", i + 1);
            }
        }
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
        cs.rebuild_composite();
        Log("Timeline: added frame %d", cs.active_frame + 1);
    }

    ImGui::EndChild();

    ImGui::End();
}
