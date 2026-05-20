#include "workbench.h"
#include "imgui_internal.h"

ImGuiID BeginWorkbench() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    constexpr ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##Workbench", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    ImGuiID id = ImGui::GetID("WorkbenchDockSpace");
    ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);
    ImGui::End();
    return id;
}

static void BuildDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    // Bottom strip for Timeline (~20%)
    ImGuiID node_main, node_timeline;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.20f, &node_timeline, &node_main);

    // Left strip for Tools (~7% of remaining)
    ImGuiID node_center, node_tools;
    node_center = ImGui::DockBuilderSplitNode(node_main, ImGuiDir_Left, 0.07f, &node_tools, &node_center);

    // Right strip for Layers + Palette (~19% of remaining)
    ImGuiID node_right;
    node_center = ImGui::DockBuilderSplitNode(node_center, ImGuiDir_Right, 0.19f, &node_right, &node_center);

    // Bottom of right strip for Color (~40%)
    ImGuiID node_layers, node_palette;
    ImGui::DockBuilderSplitNode(node_right, ImGuiDir_Down, 0.40f, &node_palette, &node_layers);

    ImGui::DockBuilderDockWindow("Tools", node_tools);
    ImGui::DockBuilderDockWindow("Canvas", node_center);
    ImGui::DockBuilderDockWindow("Layers", node_layers);
    ImGui::DockBuilderDockWindow("Color", node_palette);
    ImGui::DockBuilderDockWindow("Timeline", node_timeline);

    ImGui::DockBuilderFinish(dockspace_id);
}

void EnsureDefaultLayout(ImGuiID dockspace_id) {
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        BuildDefaultLayout(dockspace_id);
}
