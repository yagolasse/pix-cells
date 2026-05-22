#include "test_util.h"
#include "raster.h"
#include "canvas.h"
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
    if (!(o.get(4, 4) == CLEAR && outline_count > 0)) return false; // hollow

    Canvas f(9, 9);
    raster::draw_ellipse(f, 0, 0, 8, 8, RED, true);
    return f.get(4, 4) == RED; // filled center
}

static bool test_nn_scale_upscale() {
    std::vector<uint32_t> src = { 10, 11, 12, 13 }; // 2x2: [A,B / C,D]
    std::vector<uint32_t> dst;
    raster::nn_scale(src, 2, 2, dst, 4, 4);
    return dst[0 * 4 + 0] == 10 && dst[0 * 4 + 3] == 11 &&
           dst[3 * 4 + 0] == 12 && dst[3 * 4 + 3] == 13;
}

static bool test_nn_scale_downscale() {
    std::vector<uint32_t> src(16);
    for (int i = 0; i < 16; i++) src[i] = (uint32_t)i; // 4x4
    std::vector<uint32_t> dst;
    raster::nn_scale(src, 4, 4, dst, 2, 2);
    return dst[0] == 0 && dst[1] == 2 && dst[2] == 8 && dst[3] == 10;
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
    return t.result();
}
