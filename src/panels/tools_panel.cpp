#include "tools_panel.h"
#include "imgui.h"

void panels::DrawTools(ToolsState& state) {
    ImGui::Begin("Tools");
    ImGui::Text("Brush");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##size", &state.brush_size, 1, 32, "Size: %d");
    ImGui::End();
}
