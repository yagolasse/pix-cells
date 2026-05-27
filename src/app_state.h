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
    bool        locked  = false;
    float       opacity = 1.0f;
    uint8_t     blend_mode = 0;
};

struct Frame {
    std::vector<Layer> layers;
    uint16_t           duration_ms = 100;
};

struct AnimTag {
    std::string name  = "tag";
    int         start = 0;   // inclusive frame index
    int         end   = 0;   // inclusive frame index
};

struct HistoryState {
    std::vector<Frame> frames;
    int                active_frame = 0;
    int                active_layer = 0;
};

struct CanvasState {
    std::vector<Frame>             frames;
    std::vector<AnimTag>           tags;
    int                            active_tag   = -1; // -1 = play all frames
    int                            active_frame = 0;
    int                            active_layer = 0;
    float                          fps          = 12.0f;
    std::vector<uint32_t>          composite;   // blended result for GPU upload
    float                          zoom         = 8.0f;
    ImVec2                         pan          = { 0.0f, 0.0f };
    bool                           dirty        = true;
    bool                           needs_center = true;
    float                          checker_size   = 8.0f;
    ImVec4                         checker_color1 = { 220/255.f, 220/255.f, 220/255.f, 1.f };
    ImVec4                         checker_color2 = { 170/255.f, 170/255.f, 170/255.f, 1.f };

    std::deque<HistoryState>       undo_stack;  // frame stack + selection snapshots
    std::deque<HistoryState>       redo_stack;
    static constexpr int           MAX_HISTORY = 50;
    bool                           unsaved_changes = false;

    std::vector<Layer>&       active_layers()       { return frames[active_frame].layers; }
    const std::vector<Layer>& active_layers() const { return frames[active_frame].layers; }

    bool active_layer_locked() const {
        return frames[active_frame].layers[active_layer].locked;
    }

    CanvasState();

    int     width()  const;
    int     height() const;
    Canvas& active();

    void composite_frame(int frame_idx, std::vector<uint32_t>& out) const;
    void rebuild_composite();
    void push_snapshot();
    void undo();
    void redo();
    void new_canvas(int w, int h);
    void duplicate_frame(int idx);
    void delete_frame(int idx);
};

struct ToolsState {
    int  active_tool  = 0; // see tool:: constants below
    int  brush_size   = 1;
    bool circle_brush = false; // circular stamp for Brush/Eraser
    bool shape_filled = false; // Rect/Circle: filled vs outline
    bool show_grid    = false;
    bool symmetry      = false;
    int  symmetry_mode = 0; // 0=Horizontal, 1=Vertical, 2=Both
    bool onion_skin = false;
    int  onion_skin_mode = 0; // 0=Both, 1=Previous only, 2=Next only
    bool mouse_over_canvas = false;
    bool show_preview = true;
};

namespace tool {
    constexpr int Brush = 0, Eraser = 1, Fill = 2, Line = 3,
                  Rect = 4, FilledRect = 5, Circle = 6, FilledCircle = 7,
                  Move = 8, RectSelect = 9, ColorPicker = 10;
    constexpr bool is_shape(int t) { return t >= Rect && t <= FilledCircle; }
}

struct SelectionState {
    bool active      = false;
    int  x0 = 0, y0 = 0;        // top-left (canvas pixels, normalized)
    int  x1 = 0, y1 = 0;        // bottom-right (inclusive)

    // Floating selection (pixels lifted from canvas, following the mouse)
    bool floating        = false;
    std::vector<uint32_t> float_pixels;
    int  float_w = 0, float_h = 0;
    int  float_x = 0, float_y = 0;
    int  float_orig_x = 0, float_orig_y = 0;

    std::vector<uint32_t> clipboard;
    int  clipboard_w  = 0, clipboard_h  = 0;
    int  clipboard_ox = 0, clipboard_oy = 0; // paste origin

    int  width()  const { return x1 - x0 + 1; }
    int  height() const { return y1 - y0 + 1; }
    bool contains(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

struct PaletteState {
    ImVec4 primary_color   { 0.0f, 0.0f, 0.0f, 1.0f };
    ImVec4 secondary_color { 1.0f, 1.0f, 1.0f, 1.0f };

    std::vector<ImVec4> swatches;
    int                 selected_swatch = -1;
    std::string         palette_name    = "pico-8";
    std::vector<ImVec4> recent_colors;  // most-recent first, capped at 8

    PaletteState();
};

struct AppState {
    CanvasState    canvas;
    ToolsState     tools;
    PaletteState   palette;
    SelectionState selection;
    std::string    project_path;  // empty = untitled
};

void lift_selection(CanvasState& cs, SelectionState& sel);
void commit_floating(CanvasState& cs, SelectionState& sel);

inline int   opacity_pct(float f) { return static_cast<int>(f * 100.0f + 0.5f); }
inline float pct_opacity(int p)   { return p / 100.0f; }
