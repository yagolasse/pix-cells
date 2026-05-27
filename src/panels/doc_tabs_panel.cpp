#include "doc_tabs_panel.h"
#include "imgui.h"

void panels::DrawDocTabs(AppState& app) {
    ImGui::Begin("Documents", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::BeginTabBar("##doctabs",
                           ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        // Track the last active_doc set from within this panel. When app.active_doc differs,
        // it means an external change (new doc, close, open) — apply SetSelected once to sync.
        static int s_panel_active = -1;

        for (int i = 0; i < (int)app.docs.size(); i++) {
            const Document& d = app.docs[i];

            std::string base;
            if (d.project_path.empty()) {
                base = "Untitled";
            } else {
                auto pos = d.project_path.find_last_of("/\\");
                base = (pos == std::string::npos) ? d.project_path
                                                  : d.project_path.substr(pos + 1);
            }
            // ##i suffix ensures uniqueness when multiple Untitled tabs exist
            std::string label = (d.canvas.unsaved_changes ? base + "*" : base)
                              + "##" + std::to_string(i);

            bool tab_open = true;
            ImGuiTabItemFlags flags = (i == app.active_doc && app.active_doc != s_panel_active)
                                          ? ImGuiTabItemFlags_SetSelected
                                          : 0;
            if (ImGui::BeginTabItem(label.c_str(), &tab_open, flags)) {
                s_panel_active = i;
                app.active_doc = i;
                ImGui::EndTabItem();
            }
            if (!tab_open)
                app.close_doc_idx_requested = i;
        }

        // "+" trailing button — creates a new blank document
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            app.docs.emplace_back();
            app.active_doc = (int)app.docs.size() - 1;
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
