#pragma once
#include "imgui.h"

struct CanvasState  { int zoom = 1; };
struct ToolsState   { int active_tool = 0; };
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
