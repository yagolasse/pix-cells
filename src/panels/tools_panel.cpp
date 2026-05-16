#include "tools_panel.h"
#include "imgui.h"

void panels::DrawTools(ToolsState& state) {
    ImGui::Begin("Tools");

    const char* names[] = { "Brush", "Eraser" };
    for (int i = 0; i < 2; i++) {
        bool sel = (state.active_tool == i);
        if (sel) ImGui::PushStyleColor(ImGuiCol_Button,
                      ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(names[i], ImVec2(-1, 0))) state.active_tool = i;
        if (sel) ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##size", &state.brush_size, 1, 32, "Size: %d");

    ImGui::End();
}
