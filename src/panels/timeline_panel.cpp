#include "timeline_panel.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <cstdio>
#include <cstring>

static ImFont* s_icon_font = nullptr;
void panels::SetTimelineIconFont(ImFont* font) { s_icon_font = font; }

static int  s_active_frame = 2;
static int  s_frame_count  = 8;
static int  s_fps          = 12;
static int  s_frame_ms[32] = {100,100,100,100,100,100,100,100};

static const ImU32 k_checker_a = IM_COL32(58,  52, 44, 255); // #3a342c
static const ImU32 k_checker_b = IM_COL32(42,  37, 32, 255); // #2a2520
static const ImU32 k_badge_bg  = IM_COL32(0,    0,  0, 140); // rgba(0,0,0,0.55)

static void draw_frame_card(ImDrawList* dl, int idx, bool active) {
    const float SZ = 68.0f;
    char id[16];
    snprintf(id, sizeof(id), "##fc%d", idx);
    ImGui::InvisibleButton(id, {SZ, SZ});
    if (ImGui::IsItemClicked()) s_active_frame = idx;

    ImVec2 p  = ImGui::GetItemRectMin();
    ImVec2 p2 = {p.x + SZ, p.y + SZ};

    // Background
    ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_ChildBg);
    dl->AddRectFilled(p, p2, col_bg, 4.0f);

    // Checkerboard (2×2 grid of 17px cells)
    const float cs = 17.0f;
    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 4; cx++) {
            ImU32 cc = ((cx + cy) % 2 == 0) ? k_checker_a : k_checker_b;
            ImVec2 cp  = {p.x + cx * cs, p.y + cy * cs};
            ImVec2 cp2 = {cp.x + cs, cp.y + cs};
            // Clip to card bounds
            if (cp2.x > p2.x) cp2.x = p2.x;
            if (cp2.y > p2.y) cp2.y = p2.y;
            dl->AddRectFilled(cp, cp2, cc);
        }
    }

    // Border
    if (active) {
        ImU32 accent = ImGui::GetColorU32(ImGuiCol_SliderGrab);
        dl->AddRect({p.x + 1, p.y + 1}, {p2.x - 1, p2.y - 1}, accent, 4.0f, 0, 2.0f);
    } else {
        dl->AddRect(p, p2, ImGui::GetColorU32(ImGuiCol_Border), 4.0f, 0, 1.0f);
    }

    // Frame number badge — top left
    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%02d", idx + 1);
    ImVec2 num_sz = ImGui::CalcTextSize(num_buf);
    ImVec2 nb_min = {p.x + 3, p.y + 3};
    ImVec2 nb_max = {nb_min.x + num_sz.x + 6, nb_min.y + num_sz.y + 2};
    dl->AddRectFilled(nb_min, nb_max, k_badge_bg, 2.0f);
    dl->AddText({nb_min.x + 3, nb_min.y + 1}, ImGui::GetColorU32(ImGuiCol_Text), num_buf);

    // Duration badge — bottom right
    char dur_buf[16];
    snprintf(dur_buf, sizeof(dur_buf), "%dms", s_frame_ms[idx < 32 ? idx : 0]);
    ImVec2 dur_sz = ImGui::CalcTextSize(dur_buf);
    ImVec2 db_max = {p2.x - 3, p2.y - 3};
    ImVec2 db_min = {db_max.x - dur_sz.x - 6, db_max.y - dur_sz.y - 2};
    dl->AddRectFilled(db_min, db_max, k_badge_bg, 2.0f);
    dl->AddText({db_min.x + 3, db_min.y + 1}, IM_COL32_WHITE, dur_buf);
}

static void draw_add_card(ImDrawList* dl) {
    const float SZ = 68.0f;
    ImGui::InvisibleButton("##fcadd", {SZ, SZ});
    bool hov = ImGui::IsItemHovered();

    ImVec2 p  = ImGui::GetItemRectMin();
    ImVec2 p2 = {p.x + SZ, p.y + SZ};

    ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_ChildBg);
    dl->AddRectFilled(p, p2, col_bg, 4.0f);
    dl->AddRect(p, p2, ImGui::GetColorU32(hov ? ImGuiCol_BorderShadow : ImGuiCol_Border), 4.0f, 0, 1.0f);

    // Centered "+" text
    const char* plus = "+";
    ImVec2 ts = ImGui::CalcTextSize(plus);
    ImVec2 tc = {p.x + (SZ - ts.x) * 0.5f, p.y + (SZ - ts.y) * 0.5f};
    dl->AddText(tc, ImGui::GetColorU32(ImGuiCol_TextDisabled), plus);
}

void panels::DrawTimeline() {
    ImGui::Begin("Timeline");

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float btn_sz = 22.0f;
    const float sp    = ImGui::GetStyle().ItemSpacing.x;

    // ── Transport buttons ──────────────────────────────────────────────
    if (s_icon_font) ImGui::PushFont(s_icon_font);
    if (ImGui::Button(ICON_FA_BACKWARD_STEP "##tfirst", {btn_sz, btn_sz})) s_active_frame = 0;
    if (s_icon_font) { ImGui::PopFont(); }
    if (ImGui::IsItemHovered()) { ImGui::PushFont(nullptr); ImGui::SetTooltip("First"); ImGui::PopFont(); }

    ImGui::SameLine(0, sp);
    if (s_icon_font) ImGui::PushFont(s_icon_font);
    ImGui::Button(ICON_FA_PLAY "##tplay", {btn_sz, btn_sz});
    if (s_icon_font) ImGui::PopFont();
    if (ImGui::IsItemHovered()) { ImGui::PushFont(nullptr); ImGui::SetTooltip("Play"); ImGui::PopFont(); }

    ImGui::SameLine(0, sp);
    if (s_icon_font) ImGui::PushFont(s_icon_font);
    if (ImGui::Button(ICON_FA_FORWARD_STEP "##tlast", {btn_sz, btn_sz})) s_active_frame = s_frame_count - 1;
    if (s_icon_font) ImGui::PopFont();
    if (ImGui::IsItemHovered()) { ImGui::PushFont(nullptr); ImGui::SetTooltip("Last"); ImGui::PopFont(); }

    ImGui::SameLine(0, sp * 2);
    ImGui::TextDisabled("%d frames \xc2\xb7 %d fps", s_frame_count, s_fps);

    // ── Playhead row ───────────────────────────────────────────────────
    ImGui::Spacing();
    {
        char frame_lbl[12];
        snprintf(frame_lbl, sizeof(frame_lbl), "%02d / %02d", s_active_frame + 1, s_frame_count);
        ImGui::TextDisabled("%s", frame_lbl);

        ImGui::SameLine(0, sp);
        float time_lbl_w = ImGui::CalcTextSize("0.00s").x + sp;
        ImGui::SetNextItemWidth(avail - ImGui::GetCursorPosX() + ImGui::GetStyle().WindowPadding.x - time_lbl_w);
        ImGui::SliderInt("##tph", &s_active_frame, 0, s_frame_count - 1, "");

        ImGui::SameLine(0, sp);
        float secs = (s_active_frame + 1) * (s_frame_ms[0] / 1000.0f);
        char time_buf[12];
        snprintf(time_buf, sizeof(time_buf), "%.2fs", secs);
        ImGui::TextDisabled("%s", time_buf);
    }

    // ── Frame strip ────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::BeginChild("##tstrip", {-1, 88}, false, ImGuiWindowFlags_HorizontalScrollbar);
    dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < s_frame_count; i++) {
        draw_frame_card(dl, i, i == s_active_frame);
        ImGui::SameLine(0, 6);
    }
    draw_add_card(dl);

    ImGui::EndChild();

    ImGui::End();
}
