#pragma once
#include "app_state.h"
#include "imgui.h"

namespace panels {
void SetLayersIconFont(ImFont* font);
void DrawLayers(CanvasState& cs);
} // namespace panels
