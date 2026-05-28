#pragma once
#include "canvas_panel_internal.h"

// All state needed for a single frame of canvas tool input handling.
struct CanvasToolInputCtx {
    CanvasState&    cs;
    ToolsState&     tools;
    SelectionState& sel;
    PaletteState&   palette;
    DocRenderState& drs;
    ImDrawList*     dl;
    ImVec2          base;     // screen anchor for pan/zoom math (fixed per frame)
    ImVec2          origin;   // screen-space canvas origin (updated on scroll)
    int             px;       // canvas-coord mouse X (updated on scroll)
    int             py;       // canvas-coord mouse Y (updated on scroll)
    bool            mouse_in_win;
    bool            any_popup;
    const ImVec2*   hpos;     // 8 handle positions from draw_canvas_decorations
    float           W;        // canvas width in screen pixels
    float           H;        // canvas height in screen pixels
};

// Processes all tool input for one DrawCanvas frame: shape preview overlay,
// brush/fill/picker input inside the hovered region, shape/selection drag
// start via the unified window-rect check, floating selection update, handle
// drag, and shape commit on mouse release.
void handle_canvas_tool_input(CanvasToolInputCtx& ctx);
