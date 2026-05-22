#include "test_util.h"
#include "app_state.h"
#include <vector>

// CanvasState::composite_frame(idx, out) blends visible layers bottom-up
// (index 0 first) into `out`, applying per-layer opacity and blend mode.

static Layer make_layer(int w, int h, uint32_t fill) {
    Layer l;
    l.canvas = Canvas(w, h);
    l.canvas.fill(fill);
    return l;
}

static bool test_single_layer() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.active().set(2, 1, RED);
    std::vector<uint32_t> out;
    cs.composite_frame(0, out);
    return out[1 * 4 + 2] == RED && out[0] == CLEAR;
}

static bool test_invisible_layer_skipped() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.frames[0].layers[0] = make_layer(4, 4, RED);
    Layer top = make_layer(4, 4, BLUE);
    top.visible = false;
    cs.frames[0].layers.push_back(std::move(top));

    std::vector<uint32_t> out;
    cs.composite_frame(0, out);
    return out[0] == RED; // invisible blue must not show
}

static bool test_layer_order() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.frames[0].layers[0] = make_layer(4, 4, RED);
    cs.frames[0].layers.push_back(make_layer(4, 4, BLUE)); // top, opaque
    std::vector<uint32_t> out;
    cs.composite_frame(0, out);
    return out[0] == BLUE; // higher index wins
}

static bool test_opacity_blend() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.frames[0].layers[0] = make_layer(4, 4, RED);
    Layer top = make_layer(4, 4, BLUE);
    top.opacity = 0.5f;
    cs.frames[0].layers.push_back(std::move(top));

    std::vector<uint32_t> out;
    cs.composite_frame(0, out);
    uint32_t p = out[0];
    // ~half blue over opaque red: r and b near 128/127, g 0, opaque
    return approx(cR(p), 128, 6) && cG(p) == 0 && approx(cB(p), 127, 6) && cA(p) == 255;
}

static bool test_blend_mode_multiply() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.frames[0].layers[0] = make_layer(4, 4, WHITE);
    Layer top = make_layer(4, 4, RED);
    top.blend_mode = 1; // Multiply: red over white → red
    cs.frames[0].layers.push_back(std::move(top));

    std::vector<uint32_t> out;
    cs.composite_frame(0, out);
    uint32_t p = out[0];
    return approx(cR(p), 255) && approx(cG(p), 0) && approx(cB(p), 0);
}

int main() {
    TestRunner t;
    t.run("single_layer",          test_single_layer);
    t.run("invisible_layer_skipped",test_invisible_layer_skipped);
    t.run("layer_order",           test_layer_order);
    t.run("opacity_blend",         test_opacity_blend);
    t.run("blend_mode_multiply",   test_blend_mode_multiply);
    return t.result();
}
