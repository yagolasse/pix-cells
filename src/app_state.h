#pragma once
#include "imgui.h"
#include "canvas.h"
#include <vector>
#include <deque>
#include <string>

struct Layer {
    Canvas canvas;
    std::string name = "Layer";
    bool visible     = true;
    bool locked      = false;
    float opacity    = 1.0f; // 0.0–1.0; shown as 0–100% in UI
    int blend_mode   = 0;    // 0=Normal 1=Multiply 2=Screen 3=Overlay 4=Add
};

struct CanvasState {
    std::vector<Layer> layers;
    int active_layer = 0;
    std::vector<uint32_t> composite; // blended result for GPU upload
    float zoom            = 8.0f;
    ImVec2 pan            = {0.0f, 0.0f};
    bool dirty            = true;
    bool needs_center     = true;
    float checker_size    = 8.0f;
    ImVec4 checker_color1 = {220 / 255.f, 220 / 255.f, 220 / 255.f, 1.f};
    ImVec4 checker_color2 = {170 / 255.f, 170 / 255.f, 170 / 255.f, 1.f};

    std::deque<std::vector<Layer>> undo_stack; // full layer-stack snapshots
    std::deque<std::vector<Layer>> redo_stack;
    static constexpr int MAX_HISTORY = 50;

    CanvasState();

    int width() const;
    int height() const;
    Canvas& active();

    void rebuild_composite();
    void push_snapshot();
    void undo();
    void redo();
    void new_canvas(int w, int h);
};

struct ToolsState {
    int active_tool   = 0; // 0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=Circle 6=Move
    int brush_size    = 1;
    bool circle_brush = false; // circular stamp for Brush/Eraser
    bool shape_filled = false; // Rect/Circle: filled vs outline
    bool show_grid    = false;
};

struct PaletteState {
    ImVec4 primary_color{0.0f, 0.0f, 0.0f, 1.0f};
    ImVec4 secondary_color{1.0f, 1.0f, 1.0f, 1.0f};

    std::vector<ImVec4> swatches;
    int selected_swatch      = -1;
    std::string palette_name = "pico-8";
    std::vector<ImVec4> recent_colors; // most-recent first, capped at 8

    PaletteState();
};

struct AppState {
    CanvasState canvas;
    ToolsState tools;
    PaletteState palette;
};
