#include "test_util.h"
#include "raster.h"
#include "canvas.h"
#include <algorithm>
#include <vector>

static bool test_paint_pixel_single() {
    Canvas c(8, 8);
    raster::paint_pixel(c, 4, 4, RED, 1, false);
    return c.get(4, 4) == RED && c.get(3, 4) == CLEAR && c.get(4, 3) == CLEAR;
}

static bool test_paint_pixel_circle() {
    Canvas c(8, 8);
    raster::paint_pixel(c, 4, 4, RED, 3, true); // half=1 → plus shape, no corners
    bool plus = c.get(4, 4) == RED && c.get(4, 3) == RED && c.get(4, 5) == RED &&
                c.get(3, 4) == RED && c.get(5, 4) == RED;
    bool corners_clear = c.get(3, 3) == CLEAR && c.get(5, 5) == CLEAR &&
                         c.get(3, 5) == CLEAR && c.get(5, 3) == CLEAR;
    return plus && corners_clear;
}

static bool test_bresenham_diagonal() {
    Canvas c(8, 8);
    raster::bresenham(c, 0, 0, 5, 5, RED, 1, false);
    return c.get(0, 0) == RED && c.get(5, 5) == RED && c.get(2, 2) == RED;
}

static bool test_flood_fill_region_and_boundary() {
    Canvas c(4, 4); // all transparent
    for (int y = 0; y < 4; y++) c.set(2, y, BLUE); // vertical wall at x=2
    raster::flood_fill(c, 0, 0, RED);
    return c.get(0, 0) == RED && c.get(1, 3) == RED &&
           c.get(2, 0) == BLUE &&         // wall untouched
           c.get(3, 0) == CLEAR;          // right side unreachable
}

static bool test_flood_fill_same_color_noop() {
    Canvas c(4, 4);
    c.fill(RED);
    raster::flood_fill(c, 0, 0, RED); // old == new → immediate return, no hang
    return c.get(0, 0) == RED;
}

static bool test_draw_rect_outline() {
    Canvas c(6, 6);
    raster::draw_rect(c, 1, 1, 4, 4, RED, false);
    bool border = c.get(1, 1) == RED && c.get(4, 1) == RED &&
                  c.get(1, 4) == RED && c.get(4, 4) == RED && c.get(2, 1) == RED;
    bool hollow = c.get(2, 2) == CLEAR && c.get(3, 3) == CLEAR;
    return border && hollow;
}

static bool test_draw_rect_filled() {
    Canvas c(6, 6);
    raster::draw_rect(c, 1, 1, 4, 4, RED, true);
    return c.get(2, 2) == RED && c.get(3, 3) == RED && c.get(1, 1) == RED;
}

static bool test_rasterize_ellipse_degenerate() {
    int count = 0;
    int gx = -1, gy = -1;
    raster::rasterize_ellipse(3, 3, 3, 3, false, [&](int x, int y) { count++; gx = x; gy = y; });
    return count == 1 && gx == 3 && gy == 3;
}

static bool test_draw_ellipse_outline_vs_filled() {
    Canvas o(9, 9);
    raster::draw_ellipse(o, 0, 0, 8, 8, RED, false);
    int outline_count = 0;
    for (auto p : o.pixels) if (p == RED) outline_count++;
    if (o.get(4, 4) != CLEAR || outline_count == 0) return false; // hollow

    Canvas f(9, 9);
    raster::draw_ellipse(f, 0, 0, 8, 8, RED, true);
    return f.get(4, 4) == RED; // filled center
}

static bool test_nn_scale_upscale() {
    std::vector<uint32_t> src = { 10, 11, 12, 13 }; // 2x2: [A,B / C,D]
    std::vector<uint32_t> dst;
    raster::nn_scale(src, 2, 2, dst, 4, 4);
    return dst[(0 * 4) + 0] == 10 && dst[(0 * 4) + 3] == 11 &&
           dst[(3 * 4) + 0] == 12 && dst[(3 * 4) + 3] == 13;
}

static bool test_nn_scale_downscale() {
    std::vector<uint32_t> src(16);
    for (int i = 0; i < 16; i++) src[i] = (uint32_t)i; // 4x4
    std::vector<uint32_t> dst;
    raster::nn_scale(src, 4, 4, dst, 2, 2);
    return dst[0] == 0 && dst[1] == 2 && dst[2] == 8 && dst[3] == 10;
}

static bool test_nn_scale_zero_dst() {
    std::vector<uint32_t> src = { RED, RED, RED, RED };
    std::vector<uint32_t> dst;
    raster::nn_scale(src, 2, 2, dst, 0, 4);
    if (!dst.empty()) return false;
    raster::nn_scale(src, 2, 2, dst, 4, 0);
    return dst.empty();
}

static bool test_color_select_oob() {
    Canvas c(4, 4);
    auto r = raster::color_select(c, 10, 10, true);
    return !r.any;
}

static bool test_color_select_contiguous_basic() {
    Canvas c(4, 4);
    // 2×2 RED block at top-left, rest CLEAR
    c.set(0, 0, RED); c.set(1, 0, RED);
    c.set(0, 1, RED); c.set(1, 1, RED);
    auto r = raster::color_select(c, 0, 0, true);
    if (!r.any) return false;
    if (r.x0 != 0 || r.y0 != 0 || r.x1 != 1 || r.y1 != 1) return false;
    int bw = r.x1 - r.x0 + 1, bh = r.y1 - r.y0 + 1;
    if ((int)r.mask.size() != bw * bh) return false;
    return std::ranges::all_of(r.mask, [](bool b){ return b; });
}

static bool test_color_select_contiguous_disconnected() {
    Canvas c(4, 4);
    c.set(0, 0, RED);
    c.set(3, 3, RED);
    auto r = raster::color_select(c, 0, 0, true);
    if (!r.any) return false;
    if (r.x0 != 0 || r.y0 != 0 || r.x1 != 0 || r.y1 != 0) return false;
    if (r.mask.size() != 1 || !r.mask[0]) return false;
    // (3,3) must NOT be in the result
    int bw = r.x1 - r.x0 + 1;
    bool has_3_3 = false;
    for (int y = r.y0; y <= r.y1; y++)
        for (int x = r.x0; x <= r.x1; x++)
            if (r.mask[((y - r.y0) * bw) + (x - r.x0)] && x == 3 && y == 3)
                has_3_3 = true;
    return !has_3_3;
}

static bool test_color_select_global_disconnected() {
    Canvas c(4, 4);
    c.set(0, 0, RED);
    c.set(3, 3, RED);
    auto r = raster::color_select(c, 0, 0, false);
    if (!r.any) return false;
    if (r.x0 != 0 || r.y0 != 0 || r.x1 != 3 || r.y1 != 3) return false;
    int bw = r.x1 - r.x0 + 1;
    bool top_left = r.mask[((0 - r.y0) * bw) + (0 - r.x0)];
    bool bot_right = r.mask[((3 - r.y0) * bw) + (3 - r.x0)];
    bool center = r.mask[((2 - r.y0) * bw) + (2 - r.x0)];
    return top_left && bot_right && !center;
}

static bool test_color_select_thin_line() {
    Canvas c(8, 1);
    for (int x = 0; x < 8; x++) c.set(x, 0, RED);
    auto r = raster::color_select(c, 0, 0, true);
    if (!r.any) return false;
    if (r.x0 != 0 || r.y0 != 0 || r.x1 != 7 || r.y1 != 0) return false;
    if ((int)r.mask.size() != 8) return false;
    return std::ranges::all_of(r.mask, [](bool b){ return b; });
}

static bool test_color_select_mask_accuracy() {
    Canvas c(3, 3);
    c.fill(BLUE);
    c.set(1, 1, RED);
    auto r = raster::color_select(c, 1, 1, true);
    if (!r.any) return false;
    if (r.x0 != 1 || r.y0 != 1 || r.x1 != 1 || r.y1 != 1) return false;
    return r.mask.size() == 1 && r.mask[0];
}

int main() {
    TestRunner t;
    t.run("paint_pixel_single",          test_paint_pixel_single);
    t.run("paint_pixel_circle",          test_paint_pixel_circle);
    t.run("bresenham_diagonal",          test_bresenham_diagonal);
    t.run("flood_fill_region_boundary",  test_flood_fill_region_and_boundary);
    t.run("flood_fill_same_color_noop",  test_flood_fill_same_color_noop);
    t.run("draw_rect_outline",           test_draw_rect_outline);
    t.run("draw_rect_filled",            test_draw_rect_filled);
    t.run("rasterize_ellipse_degenerate",test_rasterize_ellipse_degenerate);
    t.run("draw_ellipse_outline_filled", test_draw_ellipse_outline_vs_filled);
    t.run("nn_scale_upscale",            test_nn_scale_upscale);
    t.run("nn_scale_downscale",          test_nn_scale_downscale);
    t.run("nn_scale_zero_dst",           test_nn_scale_zero_dst);
    t.run("color_select_oob",            test_color_select_oob);
    t.run("color_select_contiguous_basic",        test_color_select_contiguous_basic);
    t.run("color_select_contiguous_disconnected", test_color_select_contiguous_disconnected);
    t.run("color_select_global_disconnected",     test_color_select_global_disconnected);
    t.run("color_select_thin_line",      test_color_select_thin_line);
    t.run("color_select_mask_accuracy",  test_color_select_mask_accuracy);
    return t.result();
}
