#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>

struct Canvas {
    int width, height;
    std::vector<uint32_t> pixels; // RGBA8, row-major, R in bits 0-7

    Canvas(int w = 64, int h = 64)
        : width(w), height(h), pixels(w * h, 0xFFFFFFFF) {}

    bool in_bounds(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }
    void set(int x, int y, uint32_t rgba) { if (in_bounds(x, y)) pixels[y * width + x] = rgba; }
    uint32_t get(int x, int y) const     { return pixels[y * width + x]; }
    void fill(uint32_t rgba)   { std::fill(pixels.begin(), pixels.end(), rgba); }
    void resize(int w, int h)  { width = w; height = h; pixels.assign(w * h, 0xFFFFFFFF); }
};
