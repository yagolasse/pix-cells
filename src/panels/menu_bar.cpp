#include "menu_bar.h"
#include "imgui.h"

bool panels::DrawMenuBar(AppState& /*state*/) {
    bool keep_running = true;
    if (!ImGui::BeginMainMenuBar())
        return keep_running;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New",  "Ctrl+N")) {}
        if (ImGui::MenuItem("Open", "Ctrl+O")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) keep_running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return keep_running;
}
