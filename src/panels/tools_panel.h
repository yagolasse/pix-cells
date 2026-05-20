#pragma once
#include "app_state.h"
#include "imgui.h"

namespace panels {
void SetIconFont(ImFont* font);
void DrawTools(ToolsState& state);
} // namespace panels
