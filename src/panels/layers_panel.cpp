#include "layers_panel.h"
#include "log.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <algorithm>
#include <cstring>
#include <string>

static ImFont* s_icon_font = nullptr;
void panels::SetLayersIconFont(ImFont* font) {
    s_icon_font = font;
}

static ImVec4 layer_thumb(const Layer& layer) {
    const Canvas& c = layer.canvas;
    int pts[5][2]   = {{c.width / 2, c.height / 2},
                       {c.width / 4, c.height / 4},
                       {3 * c.width / 4, c.height / 4},
                       {c.width / 4, 3 * c.height / 4},
                       {3 * c.width / 4, 3 * c.height / 4}};
    for (auto& p : pts) {
        if (!c.in_bounds(p[0], p[1]))
            continue;
        uint32_t px = c.get(p[0], p[1]);
        if (((px >> 24) & 0xFF) > 128)
            return {(px & 0xFF) / 255.f, ((px >> 8) & 0xFF) / 255.f, ((px >> 16) & 0xFF) / 255.f, 1.f};
    }
    return {0.15f, 0.13f, 0.10f, 1.f};
}

void panels::DrawLayers(CanvasState& cs) {
    ImGui::Begin("Layers");

    auto& layers = cs.frames[cs.active_frame].layers;

    const float avail   = ImGui::GetContentRegionAvail().x;
    const float btn_sz  = 22.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    // ── Header: "Layers · N"  [+] [copy] [trash] ─────────────────────
    ImGui::Text("Layers \xc2\xb7 %d", (int)layers.size());
    ImGui::SameLine(avail - 3.0f * btn_sz - 2.0f * spacing + ImGui::GetStyle().WindowPadding.x);

    if (s_icon_font)
        ImGui::PushFont(s_icon_font);

    if (ImGui::Button(ICON_FA_PLUS "##ladd", {btn_sz, btn_sz})) {
        cs.push_snapshot();
        Layer l;
        l.name   = "Layer " + std::to_string(layers.size() + 1);
        l.canvas = Canvas(cs.width(), cs.height());
        layers.insert(layers.begin() + cs.active_layer + 1, std::move(l));
        cs.active_layer++;
        Log("Layer added: \"%s\"", layers[cs.active_layer].name.c_str());
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("New layer");

    ImGui::SameLine(0, spacing);
    if (ImGui::Button(ICON_FA_COPY "##ldup", {btn_sz, btn_sz})) {
        cs.push_snapshot();
        Layer copy = layers[cs.active_layer];
        copy.name += " copy";
        layers.insert(layers.begin() + cs.active_layer + 1, std::move(copy));
        cs.active_layer++;
        Log("Layer duplicated: \"%s\"", layers[cs.active_layer].name.c_str());
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Duplicate layer");

    ImGui::SameLine(0, spacing);
    ImGui::BeginDisabled((int)layers.size() <= 1);
    if (ImGui::Button(ICON_FA_TRASH "##ldel", {btn_sz, btn_sz})) {
        Log("Layer deleted: \"%s\"", layers[cs.active_layer].name.c_str());
        cs.push_snapshot();
        layers.erase(layers.begin() + cs.active_layer);
        cs.active_layer = std::min(cs.active_layer, (int)layers.size() - 1);
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Delete layer");
    ImGui::EndDisabled();

    if (s_icon_font)
        ImGui::PopFont();

    ImGui::Separator();

    // ── Layer list ─────────────────────────────────────────────────────
    static int renaming_layer   = -1;
    static char rename_buf[128] = {};
    static bool focus_rename    = false;

    if (renaming_layer >= (int)layers.size())
        renaming_layer = -1;

    const ImVec4 dim_col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    const float op_w     = ImGui::CalcTextSize("100%").x + 4.0f;

    for (int i = (int)layers.size() - 1; i >= 0; i--) {
        auto& layer = layers[i];
        ImGui::PushID(i);

        if (s_icon_font)
            ImGui::PushFont(s_icon_font);

        // Eye icon (transparent button, dimmed when hidden)
        ImGui::PushStyleColor(ImGuiCol_Text, layer.visible ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : dim_col);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
        if (ImGui::Button(layer.visible ? ICON_FA_EYE "##eye" : ICON_FA_EYE_SLASH "##eye", {18, 18})) {
            layer.visible = !layer.visible;
            Log("Layer \"%s\" %s", layer.name.c_str(), layer.visible ? "shown" : "hidden");
            cs.rebuild_composite();
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0, 3);

        // Lock icon (transparent button, dimmed when unlocked)
        ImGui::PushStyleColor(ImGuiCol_Text, layer.locked ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : dim_col);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
        if (ImGui::Button(layer.locked ? ICON_FA_LOCK "##lck" : ICON_FA_LOCK_OPEN "##lck", {18, 18})) {
            layer.locked = !layer.locked;
            Log("Layer \"%s\" %s", layer.name.c_str(), layer.locked ? "locked" : "unlocked");
        }
        ImGui::PopStyleColor(4);

        if (s_icon_font)
            ImGui::PopFont();

        ImGui::SameLine(0, 4);

        // Thumbnail (24×24 color swatch)
        ImGui::ColorButton("##th", layer_thumb(layer),
                           ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip,
                           {24, 24});

        ImGui::SameLine(0, 6);

        // Name / inline rename
        float name_w = ImGui::GetContentRegionAvail().x - op_w;
        if (renaming_layer == i) {
            if (focus_rename) {
                ImGui::SetKeyboardFocusHere();
                focus_rename = false;
            }
            ImGui::SetNextItemWidth(name_w);
            bool done   = ImGui::InputText("##rename", rename_buf, sizeof(rename_buf),
                                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape);
            if (done) {
                if (rename_buf[0]) {
                    Log("Layer renamed: \"%s\" -> \"%s\"", layer.name.c_str(), rename_buf);
                    layer.name = rename_buf;
                }
                renaming_layer = -1;
            } else if (cancel || (!ImGui::IsItemActive() && ImGui::IsItemDeactivated())) {
                renaming_layer = -1;
            }
        } else {
            if (ImGui::Selectable(layer.name.c_str(), i == cs.active_layer, 0, {name_w, 0})) {
                Log("Active layer: \"%s\"", layer.name.c_str());
                cs.active_layer = i;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                renaming_layer = i;
                strncpy(rename_buf, layer.name.c_str(), sizeof(rename_buf) - 1);
                rename_buf[sizeof(rename_buf) - 1] = '\0';
                focus_rename                       = true;
            }
        }

        // Opacity %
        ImGui::SameLine();
        ImGui::TextDisabled("%d%%", (int)(layer.opacity * 100.0f + 0.5f));

        ImGui::PopID();
    }

    // ── Properties section ─────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (cs.active_layer >= 0 && cs.active_layer < (int)layers.size()) {
        auto& layer = layers[cs.active_layer];

        ImGui::Text("Layer: \"%s\"", layer.name.c_str());
        ImGui::Spacing();

        int op_pct = (int)(layer.opacity * 100.0f + 0.5f);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##opacity", &op_pct, 0, 100, "Opacity: %d%%")) {
            layer.opacity = op_pct / 100.0f;
            cs.rebuild_composite();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Blend");
        ImGui::SameLine();
        static const char* blend_modes[] = {"Normal", "Multiply", "Screen", "Overlay", "Add"};
        ImGui::SetNextItemWidth(-1);
        int blend_idx = layer.blend_mode;
        if (ImGui::Combo("##blend", &blend_idx, blend_modes, IM_ARRAYSIZE(blend_modes))) {
            layer.blend_mode = (uint8_t)blend_idx;
            cs.rebuild_composite();
        }
    }

    ImGui::End();
}
