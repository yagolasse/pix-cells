#include "layers_panel.h"
#include "imgui.h"
#include <algorithm>
#include <string>

void panels::DrawLayers(CanvasState& cs) {
    ImGui::Begin("Layers");

    if (ImGui::Button("+")) {
        cs.push_snapshot();
        Layer l;
        l.name   = "Layer " + std::to_string(cs.layers.size() + 1);
        l.canvas = Canvas(cs.width(), cs.height());
        cs.layers.insert(cs.layers.begin() + cs.active_layer + 1, std::move(l));
        cs.active_layer++;
        cs.rebuild_composite();
    }
    ImGui::SameLine();
    if (cs.layers.size() > 1 && ImGui::Button("-")) {
        cs.push_snapshot();
        cs.layers.erase(cs.layers.begin() + cs.active_layer);
        cs.active_layer = std::min(cs.active_layer, (int)cs.layers.size() - 1);
        cs.rebuild_composite();
    }

    ImGui::Separator();

    // Draw layers top-to-bottom (highest index first)
    for (int i = (int)cs.layers.size() - 1; i >= 0; i--) {
        auto& layer = cs.layers[i];
        ImGui::PushID(i);
        if (ImGui::Checkbox("##v", &layer.visible))
            cs.rebuild_composite();
        ImGui::SameLine();
        if (ImGui::Selectable(layer.name.c_str(), i == cs.active_layer))
            cs.active_layer = i;
        ImGui::PopID();
    }

    ImGui::End();
}
