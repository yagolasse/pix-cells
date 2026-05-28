#pragma once
#include "app_state.h"
#include <SDL3/SDL_opengl.h>

// Handle movement table — which bounds change when dragging each handle.
// Handles: TL=0 TM=1 TR=2 RM=3 BR=4 BM=5 BL=6 LM=7
// Columns: {moves_x0, moves_y0, moves_x1, moves_y1}
static constexpr bool k_hmoves[8][4] = {
    {true,true,false,false}, {false,true,false,false}, {false,true,true,false}, {false,false,true,false},
    {false,false,true,true}, {false,false,false,true}, {true,false,false,true}, {true,false,false,false}
};

struct CanvasDragState {
    bool shape_dragging  = false;
    int  shape_sx = -1, shape_sy = -1;
    int  float_drag_ox = 0, float_drag_oy = 0;
    int  active_handle    = -1;
    bool handle_dragging  = false;
    int  handle_start_x0 = 0, handle_start_y0 = 0, handle_start_x1 = 0, handle_start_y1 = 0;
};

struct SelSeg { int x0, y0, x1, y1; }; // canvas-pixel coord edge segment for marching ants

// Per-document GL and input state managed by canvas_panel.cpp.
struct DocRenderState {
    GLuint texture      = 0;
    GLuint onion_tex[2] = {0, 0};
    int    tex_w = 0, tex_h = 0;
    ImVec2 last_px       = {-1.f, -1.f};
    bool   was_painting  = false;
    CanvasDragState drag;
    ImVec2 prev_avail    = {0.f, 0.f};
    uint64_t onion_prev_rev = 0;  // last uploaded revision for onion_tex[0]
    uint64_t onion_next_rev = 0;  // last uploaded revision for onion_tex[1]
    std::vector<SelSeg> ant_edges;  // pre-generated marching ants edge segments
    uint64_t ant_sel_rev = 0;       // sel.sel_revision when ant_edges was last built
};
