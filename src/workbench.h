#pragma once
#include "imgui.h"

// Returns the dockspace ID. Call once per frame before any panel Begin() calls.
ImGuiID BeginWorkbench();

// Builds the default panel layout if none exists yet (checks internally).
// Only workbench.cpp includes imgui_internal.h — callers don't need to.
void EnsureDefaultLayout(ImGuiID dockspace_id);
