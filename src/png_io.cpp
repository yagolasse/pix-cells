#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#pragma GCC diagnostic pop
#include "png_io.h"
#include "blend.h"
#include "log.h"
#include <cstring>
#include <cmath>
#include <vector>

bool png_io::save(const Canvas& c, const std::string& path) {
    return stbi_write_png(path.c_str(), c.width, c.height, 4, c.pixels.data(), c.width * 4) != 0;
}

bool png_io::load(Canvas& c, const std::string& path) {
    int w, h, ch;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data)
        return false;
    c.resize(w, h);
    std::memcpy(c.pixels.data(), data, w * h * 4);
    stbi_image_free(data);
    return true;
}

bool png_io::save_sprite_sheet(const CanvasState& cs,
                                const std::string& path,
                                SheetLayout layout,
                                int grid_cols) {
    int fw = cs.width();
    int fh = cs.height();
    int n  = (int)cs.frames.size();

    if (fw <= 0 || fh <= 0 || n <= 0) {
        Log("png_io::save_sprite_sheet: empty canvas");
        return false;
    }

    int out_w = 0, out_h = 0;
    int cols  = 0, rows  = 0;

    switch (layout) {
    case SheetLayout::Horizontal:
        out_w = fw * n;
        out_h = fh;
        cols  = n;
        rows  = 1;
        break;
    case SheetLayout::Vertical:
        out_w = fw;
        out_h = fh * n;
        cols  = 1;
        rows  = n;
        break;
    case SheetLayout::Grid:
        cols  = grid_cols > 0 ? grid_cols : 4;
        rows  = (int)std::ceil((double)n / (double)cols);
        out_w = fw * cols;
        out_h = fh * rows;
        break;
    }

    std::vector<uint32_t> sheet((size_t)(out_w * out_h), 0x00000000u);

    for (int fi = 0; fi < n; fi++) {
        // Composite this frame locally
        std::vector<uint32_t> frame_composite((size_t)(fw * fh), 0x00000000u);
        for (const auto& layer : cs.frames[fi].layers) {
            if (!layer.visible) continue;
            for (int i = 0; i < fw * fh; i++) {
                uint32_t src = layer.canvas.pixels[i];
                if (layer.opacity < 1.0f) {
                    uint8_t a = (uint8_t)(((src >> 24) & 0xFF) * layer.opacity);
                    src = (src & 0x00FFFFFF) | ((uint32_t)a << 24);
                }
                frame_composite[i] = blend_pixel(frame_composite[i], src, layer.blend_mode);
            }
        }

        // Determine blit offset in the sheet
        int col = fi % cols;
        int row = fi / cols;
        int ox  = col * fw;
        int oy  = row * fh;

        // Blit frame into sheet
        for (int y = 0; y < fh; y++) {
            for (int x = 0; x < fw; x++) {
                int sheet_idx = (oy + y) * out_w + (ox + x);
                int frame_idx = y * fw + x;
                sheet[sheet_idx] = frame_composite[frame_idx];
            }
        }
    }

    bool ok = stbi_write_png(path.c_str(), out_w, out_h, 4,
                             sheet.data(), out_w * 4) != 0;
    if (ok)
        Log("png_io::save_sprite_sheet: saved \"%s\" (%dx%d, %d frames)", path.c_str(), out_w, out_h, n);
    else
        Log("png_io::save_sprite_sheet: failed to write \"%s\"", path.c_str());
    return ok;
}
