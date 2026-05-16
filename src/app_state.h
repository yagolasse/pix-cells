#pragma once
#include "imgui.h"
#include "canvas.h"

struct CanvasState {
    Canvas canvas;
    float  zoom  = 8.0f;
    ImVec2 pan   = { 0.0f, 0.0f }; // screen-pixel offset for middle-mouse pan
    bool   dirty = true;
};

struct ToolsState {
    int active_tool = 0; // 0 = brush, 1 = eraser
    int brush_size  = 1;
};

struct PaletteState {
    ImVec4 primary_color   { 0.0f, 0.0f, 0.0f, 1.0f };
    ImVec4 secondary_color { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct LayersState  { int active_layer = 0; };

struct AppState {
    CanvasState  canvas;
    ToolsState   tools;
    PaletteState palette;
    LayersState  layers;
};
