#pragma once
#include "imgui.h"

// Returns the dockspace ID. Call once per frame before any panel Begin() calls.
// Builds the default layout automatically on first launch (when no imgui.ini exists).
ImGuiID BeginWorkbench();
