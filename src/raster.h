#pragma once
#include "canvas.h"
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace raster {

void paint_pixel(Canvas& c, int x, int y, uint32_t color, int brush_size, bool circle);
void bresenham(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color, int brush_size, bool circle);
void flood_fill(Canvas& c, int sx, int sy, uint32_t new_col);
void draw_rect(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color, bool filled);
void draw_ellipse(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color, bool filled);
void nn_scale(const std::vector<uint32_t>& src, int sw, int sh,
              std::vector<uint32_t>& dst, int dw, int dh);

struct ColorSelectResult {
    int x0, y0, x1, y1;
    std::vector<bool> mask; // size = (x1-x0+1)*(y1-y0+1), bbox-relative
    bool any;
};
ColorSelectResult color_select(const Canvas& c, int sx, int sy, bool contiguous);

// Bresenham ellipse-in-rect (Zingl). Fits the bounding box [x0,x1]×[y0,y1] exactly,
// handling odd AND even diameters, so a 1px drag change grows the shape by 1px. When
// `filled`, each scanline's left/right edge points are span-filled, so the fill is derived
// from the same edges as the outline — identical silhouette, no single-pixel pole nub.
// Defined here because it's a template used by both draw_ellipse and the canvas preview.
template <class Plot>
void rasterize_ellipse(int x0, int y0, int x1, int y1, bool filled, Plot plot) {
    if (x0 == x1 && y0 == y1) { plot(x0, y0); return; }
    long a = std::abs(x1 - x0), b = std::abs(y1 - y0), b1 = b & 1;
    long dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a;
    long err = dx + dy + b1 * a * a, e2;
    if (x0 > x1) { x0 = x1; x1 += a; }
    if (y0 > y1) y0 = y1;
    y0 += (b + 1) / 2; y1 = y0 - b1;
    a *= 8 * a; b1 = 8 * b * b;
    do {
        if (filled) {
            for (int x = x0; x <= x1; x++) { plot(x, y0); plot(x, y1); }
        } else {
            plot(x1, y0); plot(x0, y0); plot(x0, y1); plot(x1, y1);
        }
        e2 = 2 * err;
        if (e2 <= dy) { y0++; y1--; err += dy += a; }
        if (e2 >= dx || 2 * err > dy) { x0++; x1--; err += dx += b1; }
    } while (x0 <= x1);
    while (y0 - y1 < b) {  // finish the tips of flat (near-1px) ellipses
        if (filled) {
            for (int x = x0 - 1; x <= x1 + 1; x++) { plot(x, y0); plot(x, y1); }
        } else {
            plot(x0 - 1, y0); plot(x1 + 1, y0);
            plot(x0 - 1, y1); plot(x1 + 1, y1);
        }
        y0++; y1--;
    }
}

} // namespace raster
