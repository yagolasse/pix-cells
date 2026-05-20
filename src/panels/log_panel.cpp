#include "log_panel.h"
#include "log.h"
#include "imgui.h"

void panels::DrawLog(bool* p_open) {
    ImGui::Begin("Log", p_open, ImGuiWindowFlags_NoTitleBar);

    if (ImGui::Button("Clear")) LogClear();
    ImGui::Separator();

    ImGui::BeginChild("##entries", ImVec2(0, 0), ImGuiChildFlags_None);
    for (const auto& entry : LogEntries())
        ImGui::TextUnformatted(entry.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}
