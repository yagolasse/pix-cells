#include "layers_panel.h"
#include "log.h"
#include "imgui.h"
#include "icon_manager.h"
#include <algorithm>
#include <cstring>
#include <string>

static void push_ghost_style() {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1, 1, 1, 0.15f));
}
static void pop_ghost_style() { ImGui::PopStyleColor(3); }

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

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if (ImGui::ImageButton("##ladd", icon_manager::get("add"), {btn_sz, btn_sz})) {
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
    if (ImGui::ImageButton("##ldup", icon_manager::get("copy"), {btn_sz, btn_sz})) {
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
    if (ImGui::ImageButton("##ldel", icon_manager::get("delete"), {btn_sz, btn_sz})) {
        Log("Layer deleted: \"%s\"", layers[cs.active_layer].name.c_str());
        cs.push_snapshot();
        layers.erase(layers.begin() + cs.active_layer);
        cs.active_layer = std::min(cs.active_layer, (int)layers.size() - 1);
        cs.rebuild_composite();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Delete layer");
    ImGui::EndDisabled();

    ImGui::PopStyleVar();

    ImGui::Separator();

    // ── Layer list ─────────────────────────────────────────────────────
    static int renaming_layer   = -1;
    static char rename_buf[128] = {};
    static bool focus_rename    = false;

    if (renaming_layer >= (int)layers.size())
        renaming_layer = -1;

    const float op_w = ImGui::CalcTextSize("100%").x + 4.0f;

    for (int i = (int)layers.size() - 1; i >= 0; i--) {
        auto& layer = layers[i];
        ImGui::PushID(i);

        // Eye icon (transparent button)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        push_ghost_style();
        const char* eye_icon = layer.visible ? "visibility" : "visibility_off";
        if (ImGui::ImageButton("##eye", icon_manager::get(eye_icon), {20, 20})) {
            layer.visible = !layer.visible;
            Log("Layer \"%s\" %s", layer.name.c_str(), layer.visible ? "shown" : "hidden");
            cs.rebuild_composite();
        }
        pop_ghost_style();

        ImGui::SameLine(0, 5);

        // Lock icon (transparent button)
        push_ghost_style();
        const char* lock_icon = layer.locked ? "lock" : "lock_open";
        if (ImGui::ImageButton("##lck", icon_manager::get(lock_icon), {20, 20})) {
            layer.locked = !layer.locked;
            Log("Layer \"%s\" %s", layer.name.c_str(), layer.locked ? "locked" : "unlocked");
        }
        pop_ghost_style();
        ImGui::PopStyleVar();

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
                    Log(R"(Layer renamed: "%s" -> "%s")", layer.name.c_str(), rename_buf);
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
        ImGui::TextDisabled("%d%%", opacity_pct(layer.opacity));

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

        int op_pct = opacity_pct(layer.opacity);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##opacity", &op_pct, 0, 100, "Opacity: %d%%")) {
            layer.opacity = pct_opacity(op_pct);
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
