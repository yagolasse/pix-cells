#include "layers_panel.h"
#include "log.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <string>

void panels::DrawLayers(CanvasState& cs) {
    ImGui::Begin("Layers");

    if (ImGui::Button("+")) {
        cs.push_snapshot();
        Layer l;
        l.name   = "Layer " + std::to_string(cs.layers.size() + 1);
        l.canvas = Canvas(cs.width(), cs.height());
        Log("Layer added: \"%s\"", l.name.c_str());
        cs.layers.insert(cs.layers.begin() + cs.active_layer + 1, std::move(l));
        cs.active_layer++;
        cs.rebuild_composite();
    }
    ImGui::SameLine();
    if (cs.layers.size() > 1 && ImGui::Button("-")) {
        Log("Layer deleted: \"%s\"", cs.layers[cs.active_layer].name.c_str());
        cs.push_snapshot();
        cs.layers.erase(cs.layers.begin() + cs.active_layer);
        cs.active_layer = std::min(cs.active_layer, (int)cs.layers.size() - 1);
        cs.rebuild_composite();
    }

    ImGui::Separator();

    static int  renaming_layer = -1;
    static char rename_buf[128] = {};
    static bool focus_rename    = false;

    // Guard against stale rename index after layer deletion
    if (renaming_layer >= (int)cs.layers.size()) renaming_layer = -1;

    // Draw layers top-to-bottom (highest index first)
    for (int i = (int)cs.layers.size() - 1; i >= 0; i--) {
        auto& layer = cs.layers[i];
        ImGui::PushID(i);

        if (ImGui::Checkbox("##v", &layer.visible)) {
            Log("Layer \"%s\" %s", layer.name.c_str(), layer.visible ? "shown" : "hidden");
            cs.rebuild_composite();
        }
        ImGui::SameLine();

        if (renaming_layer == i) {
            if (focus_rename) { ImGui::SetKeyboardFocusHere(); focus_rename = false; }
            ImGui::SetNextItemWidth(-1);
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
            if (ImGui::Selectable(layer.name.c_str(), i == cs.active_layer)) {
                Log("Active layer: \"%s\"", layer.name.c_str());
                cs.active_layer = i;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                renaming_layer = i;
                strncpy(rename_buf, layer.name.c_str(), sizeof(rename_buf) - 1);
                rename_buf[sizeof(rename_buf) - 1] = '\0';
                focus_rename = true;
            }
        }

        ImGui::PopID();
    }

    ImGui::End();
}
