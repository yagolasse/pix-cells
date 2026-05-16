#include "palette_panel.h"
#include "imgui.h"

void panels::DrawPalette(PaletteState& state) {
    ImGui::Begin("Palette");
    ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit4("Primary##col",   (float*)&state.primary_color,
                      ImGuiColorEditFlags_NoAlpha);
    ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit4("Secondary##col", (float*)&state.secondary_color,
                      ImGuiColorEditFlags_NoAlpha);
    ImGui::End();
}
