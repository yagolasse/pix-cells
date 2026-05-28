#pragma once
#include "canvas_panel_internal.h"

// Renders the checkerboard background, onion skin layers, and sampler switch.
void draw_canvas_overlays(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
                          const GLuint* onion_tex, ImVec2 origin, float W, float H);

// Renders the pixel grid, symmetry axis guides, marching-ants selection overlay,
// floating selection pixel preview, and resize handles.
// hpos[8] is filled with the screen positions of the 8 resize handles.
void draw_canvas_decorations(ImDrawList* dl, CanvasState& cs, const ToolsState& tools,
                              const SelectionState& sel,
                              ImVec2 origin, float W, float H,
                              const CanvasDragState& drag, ImVec2* hpos);
