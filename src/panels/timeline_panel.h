#pragma once
#include "imgui.h"
#include "app_state.h"

namespace panels {
void SetTimelineIconFont(ImFont* font);
void DrawTimeline(CanvasState& cs);
} // namespace panels
