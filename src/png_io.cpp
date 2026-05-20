#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "png_io.h"
#include "log.h"
#include <cstring>
#include <cmath>
#include <vector>

bool png_io::save(const Canvas& c, const std::string& path) {
    return stbi_write_png(path.c_str(), c.width, c.height, 4,
                          c.pixels.data(), c.width * 4) != 0;
}

bool png_io::load(Canvas& c, const std::string& path) {
    int w, h, ch;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return false;
    c.resize(w, h);
    std::memcpy(c.pixels.data(), data, w * h * 4);
    stbi_image_free(data);
    return true;
}

// Porter-Duff "over" for per-frame compositing (local copy to avoid state mutation)
static uint32_t sheet_blend_over(uint32_t dst, uint32_t src) {
    float sa = ((src >> 24) & 0xFF) / 255.0f;
    if (sa <= 0.0f) return dst;
    if (sa >= 1.0f) return src;

    float da = ((dst >> 24) & 0xFF) / 255.0f;
    float sr = ( src        & 0xFF) / 255.0f;
    float sg = ((src >>  8) & 0xFF) / 255.0f;
    float sb = ((src >> 16) & 0xFF) / 255.0f;
    float dr = ( dst        & 0xFF) / 255.0f;
    float dg = ((dst >>  8) & 0xFF) / 255.0f;
    float db = ((dst >> 16) & 0xFF) / 255.0f;

    float oa = sa + da * (1.0f - sa);
    if (oa == 0.0f) return 0;
    float or_ = (sr * sa + dr * da * (1.0f - sa)) / oa;
    float og  = (sg * sa + dg * da * (1.0f - sa)) / oa;
    float ob  = (sb * sa + db * da * (1.0f - sa)) / oa;

    return ((uint32_t)(oa  * 255.0f) << 24)
         | ((uint32_t)(ob  * 255.0f) << 16)
         | ((uint32_t)(og  * 255.0f) <<  8)
         |  (uint32_t)(or_ * 255.0f);
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
            for (int i = 0; i < fw * fh; i++)
                frame_composite[i] = sheet_blend_over(frame_composite[i], layer.canvas.pixels[i]);
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
