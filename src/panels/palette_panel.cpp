#include "palette_panel.h"
#include "imgui.h"
#include "icon_manager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

// Labelled float drag (H/S/V style: accent-colored single-letter + full-width input)
static bool num_row(const char* id, const char* label, float* v, float speed, float lo, float hi, const char* fmt) {
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), "%s", label);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    return ImGui::DragFloat(id, v, speed, lo, hi, fmt);
}

// Labelled int drag (R/G/B style)
static bool channel_row(const char* id, const char* label, int* v) {
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), "%s", label);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    return ImGui::DragInt(id, v, 1.0f, 0, 255);
}

static void recent_row(PaletteState& state) {
    if (state.recent_colors.empty())
        return;
    ImGui::Spacing();
    ImGui::TextDisabled("RECENT");
    const float avail = ImGui::GetContentRegionAvail().x;
    const int n       = (int)state.recent_colors.size();
    const float rsz   = (avail - (float)(n - 1) * 3.0f) / (float)n;
    for (int i = 0; i < n; i++) {
        if (i > 0)
            ImGui::SameLine(0, 3);
        ImGui::PushID(20000 + i);
        if (ImGui::ColorButton("##rc", state.recent_colors[i],
                               ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoBorder |
                                   ImGuiColorEditFlags_NoTooltip,
                               {rsz, rsz})) {
            state.primary_color   = state.recent_colors[i];
            state.primary_color.w = 1.0f;
        }
        ImGui::PopID();
    }
}

void panels::DrawPalette(PaletteState& state) {
    ImGui::Begin("Color");

    const float avail = ImGui::GetContentRegionAvail().x;
    auto* dl          = ImGui::GetWindowDrawList();

    // ── A. Current / Previous swatches + Swap ─────────────────────────────
    {
        const float swap_w = 26.0f;
        const float h      = 34.0f;
        const float sw_w   = avail - swap_w - ImGui::GetStyle().ItemSpacing.x;
        ImVec2 p           = ImGui::GetCursorScreenPos();

        dl->AddRectFilled(p, {p.x + sw_w * 0.5f, p.y + h}, ImGui::ColorConvertFloat4ToU32(state.primary_color), 3.0f,
                          ImDrawFlags_RoundCornersLeft);
        dl->AddRectFilled({p.x + sw_w * 0.5f, p.y}, {p.x + sw_w, p.y + h},
                          ImGui::ColorConvertFloat4ToU32(state.secondary_color), 3.0f, ImDrawFlags_RoundCornersRight);
        dl->AddRect(p, {p.x + sw_w, p.y + h}, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);

        ImGui::InvisibleButton("##swatches", {sw_w, h});
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::ImageButton("##swap", icon_manager::get("swap"), {swap_w, h}))
            std::swap(state.primary_color, state.secondary_color);
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Swap colors");
    }

    // ── B. Tabs ───────────────────────────────────────────────────────────
    static char hex_buf[8]  = "F6EFE0";
    static bool hex_editing = false;

    if (ImGui::BeginTabBar("##ctabs")) {

        // ── HSV ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("HSV")) {
            ImGui::SetNextItemWidth(-1);
            float saved_alpha = state.primary_color.w;
            bool  picker_used = ImGui::ColorPicker4("##picker", (float*)&state.primary_color,
                                                    ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs |
                                                        ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_PickerHueBar |
                                                        ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoLabel);
            if (picker_used)
                state.primary_color.w = 1.0f;
            else
                state.primary_color.w = saved_alpha;

            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(state.primary_color.x, state.primary_color.y, state.primary_color.z, h, s, v);
            float h_deg = h * 360.0f;
            float s_pct = s * 100.0f;
            float v_pct = v * 100.0f;
            float a_pct = 100.0f;

            bool changed = false;
            if (num_row("##hv", "H", &h_deg, 0.5f, 0.0f, 360.0f, "%.0f\xc2\xb0"))
                changed = true;
            if (num_row("##sv", "S", &s_pct, 0.3f, 0.0f, 100.0f, "%.0f%%"))
                changed = true;
            if (num_row("##vv", "V", &v_pct, 0.3f, 0.0f, 100.0f, "%.0f%%"))
                changed = true;
            ImGui::BeginDisabled();
            num_row("##av", "A", &a_pct, 0.0f, 100.0f, 100.0f, "%.0f%%");
            ImGui::EndDisabled();

            if (changed) {
                ImGui::ColorConvertHSVtoRGB(h_deg / 360.0f, s_pct / 100.0f, v_pct / 100.0f, state.primary_color.x,
                                            state.primary_color.y, state.primary_color.z);
                state.primary_color.w = 1.0f;
            }

            recent_row(state);
            ImGui::EndTabItem();
        }

        // ── RGB ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("RGB")) {
            int r = (int)(state.primary_color.x * 255.0f + 0.5f);
            int g = (int)(state.primary_color.y * 255.0f + 0.5f);
            int b = (int)(state.primary_color.z * 255.0f + 0.5f);
            int a = 255;

            bool changed = false;
            if (channel_row("##cr", "R", &r))
                changed = true;
            if (channel_row("##cg", "G", &g))
                changed = true;
            if (channel_row("##cb", "B", &b))
                changed = true;
            ImGui::BeginDisabled();
            channel_row("##ca", "A", &a);
            ImGui::EndDisabled();

            if (changed)
                state.primary_color = {std::clamp(r, 0, 255) / 255.0f, std::clamp(g, 0, 255) / 255.0f,
                                       std::clamp(b, 0, 255) / 255.0f, 1.0f};

            recent_row(state);
            ImGui::EndTabItem();
        }

        // ── HEX ───────────────────────────────────────────────────────────
        bool hex_tab = ImGui::BeginTabItem("HEX");
        if (!hex_tab)
            hex_editing = false;
        if (hex_tab) {
            int r = (int)(state.primary_color.x * 255.0f + 0.5f);
            int g = (int)(state.primary_color.y * 255.0f + 0.5f);
            int b = (int)(state.primary_color.z * 255.0f + 0.5f);

            if (!hex_editing)
                snprintf(hex_buf, sizeof(hex_buf), "%02X%02X%02X", r, g, b);

            // # hex input + Copy
            const float copy_w = ImGui::CalcTextSize("Copy").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            const float lbl_w  = ImGui::CalcTextSize("#").x + 2.0f;
            ImGui::Text("#");
            ImGui::SameLine(0, 2);
            ImGui::SetNextItemWidth(avail - lbl_w - copy_w - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::InputText("##hex", hex_buf, sizeof(hex_buf),
                                 ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
                if (strlen(hex_buf) == 6) {
                    unsigned int parsed = 0;
                    sscanf(hex_buf, "%X", &parsed);
                    state.primary_color = {((parsed >> 16) & 0xFF) / 255.0f, ((parsed >> 8) & 0xFF) / 255.0f,
                                           (parsed & 0xFF) / 255.0f, 1.0f};
                }
            }
            hex_editing = ImGui::IsItemActive();
            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
                char clip[10];
                snprintf(clip, sizeof(clip), "#%s", hex_buf);
                ImGui::SetClipboardText(clip);
            }

            // 0x read-only row
            {
                char buf[12];
                snprintf(buf, sizeof(buf), "%02X%02X%02XFF", r, g, b);
                ImGui::TextDisabled("0x");
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##ox", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
            }

            // css read-only row
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "rgb(%d, %d, %d)", r, g, b);
                ImGui::TextDisabled("css");
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##css", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
            }

            recent_row(state);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // ── F. Palette section header ─────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    {
        ImGui::Text("Palette \xc2\xb7 %s", state.palette_name.c_str());

        const float edit_w = ImGui::CalcTextSize("edit").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - edit_w);
        if (ImGui::SmallButton("edit"))
            ImGui::OpenPopup("pal_edit_popup");

        if (ImGui::BeginPopup("pal_edit_popup")) {
            static char rename_buf[64] = "";
            if (ImGui::IsWindowAppearing())
                snprintf(rename_buf, sizeof(rename_buf), "%s", state.palette_name.c_str());
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText("Name##ren", rename_buf, sizeof(rename_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                state.palette_name = rename_buf;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // ── G. Color grid ─────────────────────────────────────────────────────
    if (!state.swatches.empty()) {
        const float gap = 3.0f;
        const float sz  = (avail - 7.0f * gap) / 8.0f;

        for (int i = 0; i < (int)state.swatches.size(); i++) {
            if (i % 8 != 0)
                ImGui::SameLine(0, (int)gap);
            ImGui::PushID(i);

            ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::ColorButton("##sw", state.swatches[i],
                                   ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoBorder |
                                       ImGuiColorEditFlags_NoTooltip,
                                   {sz, sz})) {
                const ImVec4& sc      = state.swatches[i];
                state.primary_color   = sc;
                state.primary_color.w = 1.0f;
                state.selected_swatch = i;

                state.recent_colors.erase(
                    std::remove_if(state.recent_colors.begin(), state.recent_colors.end(),
                                   [&sc](const ImVec4& c) { return c.x == sc.x && c.y == sc.y && c.z == sc.z; }),
                    state.recent_colors.end());
                state.recent_colors.insert(state.recent_colors.begin(), sc);
                if ((int)state.recent_colors.size() > 8)
                    state.recent_colors.resize(8);
            }

            if (i == state.selected_swatch)
                dl->AddRect(pos, {pos.x + sz, pos.y + sz}, ImGui::GetColorU32(ImGuiCol_ButtonActive), 2.0f, 0, 2.0f);

            ImGui::PopID();
        }
    }

    // ── H. Add / Remove / Sort ────────────────────────────────────────────
    ImGui::Spacing();

    if (ImGui::SmallButton("+ Add")) {
        const ImVec4& pc = state.primary_color;
        bool dup         = false;
        for (const auto& s : state.swatches)
            if (s.x == pc.x && s.y == pc.y && s.z == pc.z) {
                dup = true;
                break;
            }
        if (!dup) {
            ImVec4 c = pc;
            c.w      = 1.0f;
            state.swatches.push_back(c);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove")) {
        if (state.selected_swatch >= 0 && state.selected_swatch < (int)state.swatches.size()) {
            state.swatches.erase(state.swatches.begin() + state.selected_swatch);
            if (state.swatches.empty())
                state.selected_swatch = -1;
            else
                state.selected_swatch = std::min(state.selected_swatch, (int)state.swatches.size() - 1);
        }
    }

    {
        const float sort_w = ImGui::CalcTextSize("Sort").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - sort_w);
        if (ImGui::SmallButton("Sort")) {
            std::sort(state.swatches.begin(), state.swatches.end(), [](const ImVec4& a, const ImVec4& b) {
                float ah, as_, av, bh, bs_, bv;
                ImGui::ColorConvertRGBtoHSV(a.x, a.y, a.z, ah, as_, av);
                ImGui::ColorConvertRGBtoHSV(b.x, b.y, b.z, bh, bs_, bv);
                return ah < bh;
            });
            state.selected_swatch = -1;
        }
    }

    ImGui::End();
}
