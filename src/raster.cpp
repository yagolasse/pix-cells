#include "raster.h"
#include <algorithm>
#include <queue>
#include <cstdlib>

namespace raster {

void paint_pixel(Canvas& c, int x, int y, uint32_t color, int brush_size, bool circle) {
    int half = brush_size / 2;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++) {
            if (circle && dx * dx + dy * dy > half * half)
                continue;
            c.set(x + dx, y + dy, color);
        }
}

void bresenham(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color, int brush_size, bool circle) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        paint_pixel(c, x0, y0, color, brush_size, circle);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void flood_fill(Canvas& c, int sx, int sy, uint32_t new_col) {
    uint32_t old_col = c.get(sx, sy);
    if (old_col == new_col)
        return;
    std::queue<std::pair<int, int>> q;
    q.push({sx, sy});
    c.set(sx, sy, new_col);
    const int ddx[] = {1, -1, 0, 0}, ddy[] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        for (int i = 0; i < 4; i++) {
            int nx = x + ddx[i], ny = y + ddy[i];
            if (c.in_bounds(nx, ny) && c.get(nx, ny) == old_col) {
                c.set(nx, ny, new_col);
                q.push({nx, ny});
            }
        }
    }
}

void draw_rect(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color, bool filled) {
    int minx = std::min(x0, x1), maxx = std::max(x0, x1);
    int miny = std::min(y0, y1), maxy = std::max(y0, y1);
    if (filled) {
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++)
                c.set(x, y, color);
    } else {
        for (int x = minx; x <= maxx; x++) {
            c.set(x, miny, color);
            c.set(x, maxy, color);
        }
        for (int y = miny + 1; y < maxy; y++) {
            c.set(minx, y, color);
            c.set(maxx, y, color);
        }
    }
}

void draw_ellipse(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color, bool filled) {
    rasterize_ellipse(x0, y0, x1, y1, filled,
                      [&](int x, int y) { c.set(x, y, color); });
}

void nn_scale(const std::vector<uint32_t>& src, int sw, int sh,
              std::vector<uint32_t>& dst, int dw, int dh) {
    dst.resize(dw * dh);
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++)
            dst[y * dw + x] = src[(y * sh / dh) * sw + (x * sw / dw)];
}

} // namespace raster
