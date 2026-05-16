#pragma once
#include "imgui.h"
#include "canvas.h"

struct CanvasState {
    Canvas canvas;        // pixel buffer (default 64x64, white)
    float  zoom  = 8.0f; // display scale
    bool   dirty = true; // true -> re-upload to GL texture this frame
};

struct ToolsState {
    int active_tool = 0; // 0 = brush
    int brush_size  = 1; // diameter in canvas pixels
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
