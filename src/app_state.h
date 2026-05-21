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
    int  active_tool  = 0; // 0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=FilledRect 6=Circle 7=FilledCircle 8=Move 9=RectSelect 10=ColorPicker
    int  brush_size   = 1;
    bool circle_brush = false; // circular stamp for Brush/Eraser
    bool shape_filled = false; // Rect/Circle: filled vs outline
    bool show_grid    = false;
};

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
};
