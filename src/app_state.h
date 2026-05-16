#pragma once
#include "imgui.h"
#include "canvas.h"
#include <vector>
#include <deque>
#include <string>

struct Layer {
    Canvas      canvas;
    std::string name    = "Layer";
    bool        visible = true;
};

struct CanvasState {
    std::vector<Layer>             layers;
    int                            active_layer = 0;
    std::vector<uint32_t>          composite;   // blended result for GPU upload
    float                          zoom  = 8.0f;
    ImVec2                         pan   = { 0.0f, 0.0f };
    bool                           dirty = true;

    std::deque<std::vector<Layer>> undo_stack;  // full layer-stack snapshots
    std::deque<std::vector<Layer>> redo_stack;
    static constexpr int           MAX_HISTORY = 50;

    CanvasState();

    int     width()  const;
    int     height() const;
    Canvas& active();

    void rebuild_composite();
    void push_snapshot();
    void undo();
    void redo();
    void new_canvas(int w, int h);
};

struct ToolsState {
    int active_tool = 0; // 0=brush  1=eraser  2=fill
    int brush_size  = 1;
};

struct PaletteState {
    ImVec4 primary_color   { 0.0f, 0.0f, 0.0f, 1.0f };
    ImVec4 secondary_color { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct AppState {
    CanvasState  canvas;
    ToolsState   tools;
    PaletteState palette;
};
