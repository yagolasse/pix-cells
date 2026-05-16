#include "workbench.h"
#include "imgui_internal.h"

ImGuiID BeginWorkbench() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));

    constexpr ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar            | ImGuiWindowFlags_NoCollapse         |
        ImGuiWindowFlags_NoResize              | ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus         |
        ImGuiWindowFlags_NoDocking             | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##Workbench", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    ImGuiID id = ImGui::GetID("WorkbenchDockSpace");
    ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
    return id;
}

static void BuildDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    // Split left strip for Tools (~7%)
    ImGuiID node_center, node_tools;
    node_center = ImGui::DockBuilderSplitNode(
        dockspace_id, ImGuiDir_Left, 0.07f, &node_tools, &node_center);

    // Split right strip for Layers + Palette (~19%)
    ImGuiID node_right;
    node_center = ImGui::DockBuilderSplitNode(
        node_center, ImGuiDir_Right, 0.19f, &node_right, &node_center);

    // Split bottom of right strip for Palette (~40%)
    ImGuiID node_layers, node_palette;
    ImGui::DockBuilderSplitNode(
        node_right, ImGuiDir_Down, 0.40f, &node_palette, &node_layers);

    ImGui::DockBuilderDockWindow("Tools",   node_tools);
    ImGui::DockBuilderDockWindow("Canvas",  node_center);
    ImGui::DockBuilderDockWindow("Layers",  node_layers);
    ImGui::DockBuilderDockWindow("Palette", node_palette);

    ImGui::DockBuilderFinish(dockspace_id);
}

void EnsureDefaultLayout(ImGuiID dockspace_id) {
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        BuildDefaultLayout(dockspace_id);
}
