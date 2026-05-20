#include "pixc_io.h"
#include "log.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

// ---------------------------------------------------------------------------
// Helper: write/read length-prefixed strings (uint8_t length, then bytes)
// ---------------------------------------------------------------------------

static bool write_str(FILE* f, const std::string& s) {
    uint8_t len = (uint8_t)(s.size() > 255 ? 255 : s.size());
    if (fwrite(&len, 1, 1, f) != 1) return false;
    if (len > 0 && fwrite(s.data(), 1, len, f) != len) return false;
    return true;
}

static bool read_str(FILE* f, std::string& s) {
    uint8_t len = 0;
    if (fread(&len, 1, 1, f) != 1) return false;
    s.resize(len);
    if (len > 0 && fread(s.data(), 1, len, f) != len) return false;
    return true;
}

// ---------------------------------------------------------------------------
// save
// ---------------------------------------------------------------------------

bool pixc_io::save(const AppState& state, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        Log("pixc_io::save: cannot open \"%s\"", path.c_str());
        return false;
    }

    const CanvasState& cs = state.canvas;

    // HEADER
    const char magic[4] = { 'P', 'I', 'X', 'C' };
    fwrite(magic, 1, 4, f);

    uint16_t version     = 1;
    uint16_t w           = (uint16_t)cs.width();
    uint16_t h           = (uint16_t)cs.height();
    uint16_t frame_count = (uint16_t)cs.frames.size();
    float    fps         = cs.fps;

    fwrite(&version,     sizeof(uint16_t), 1, f);
    fwrite(&w,           sizeof(uint16_t), 1, f);
    fwrite(&h,           sizeof(uint16_t), 1, f);
    fwrite(&frame_count, sizeof(uint16_t), 1, f);
    fwrite(&fps,         sizeof(float),    1, f);

    // PALETTE
    const PaletteState& pal = state.palette;
    uint16_t color_count = (uint16_t)pal.swatches.size();
    fwrite(&color_count, sizeof(uint16_t), 1, f);
    for (const auto& c : pal.swatches) {
        fwrite(&c.x, sizeof(float), 1, f);
        fwrite(&c.y, sizeof(float), 1, f);
        fwrite(&c.z, sizeof(float), 1, f);
        fwrite(&c.w, sizeof(float), 1, f);
    }
    write_str(f, pal.palette_name);

    // FRAMES
    for (const auto& frame : cs.frames) {
        uint16_t dur         = frame.duration_ms;
        uint16_t layer_count = (uint16_t)frame.layers.size();
        fwrite(&dur,         sizeof(uint16_t), 1, f);
        fwrite(&layer_count, sizeof(uint16_t), 1, f);

        for (const auto& layer : frame.layers) {
            write_str(f, layer.name);
            uint8_t vis   = layer.visible  ? 1 : 0;
            uint8_t lock  = layer.locked   ? 1 : 0;
            float   op    = layer.opacity;
            uint8_t blend = layer.blend_mode;
            fwrite(&vis,   1,             1, f);
            fwrite(&lock,  1,             1, f);
            fwrite(&op,    sizeof(float), 1, f);
            fwrite(&blend, 1,             1, f);
            // pixels: width * height * 4 bytes
            fwrite(layer.canvas.pixels.data(), sizeof(uint32_t),
                   (size_t)(w * h), f);
        }
    }

    fclose(f);
    Log("pixc_io::save: saved \"%s\" (%d frames, %dx%d)", path.c_str(), (int)frame_count, (int)w, (int)h);
    return true;
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------

bool pixc_io::load(AppState& state, const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        Log("pixc_io::load: cannot open \"%s\"", path.c_str());
        return false;
    }

    // HEADER
    char magic[4] = {};
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "PIXC", 4) != 0) {
        Log("pixc_io::load: bad magic in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }

    uint16_t version = 0;
    fread(&version, sizeof(uint16_t), 1, f);
    if (version != 1) {
        Log("pixc_io::load: unsupported version %d in \"%s\"", (int)version, path.c_str());
        fclose(f);
        return false;
    }

    uint16_t w = 0, h = 0, frame_count = 0;
    float    fps = 12.0f;
    fread(&w,           sizeof(uint16_t), 1, f);
    fread(&h,           sizeof(uint16_t), 1, f);
    fread(&frame_count, sizeof(uint16_t), 1, f);
    fread(&fps,         sizeof(float),    1, f);

    // PALETTE
    uint16_t color_count = 0;
    fread(&color_count, sizeof(uint16_t), 1, f);
    state.palette.swatches.resize(color_count);
    for (auto& c : state.palette.swatches) {
        fread(&c.x, sizeof(float), 1, f);
        fread(&c.y, sizeof(float), 1, f);
        fread(&c.z, sizeof(float), 1, f);
        fread(&c.w, sizeof(float), 1, f);
    }
    read_str(f, state.palette.palette_name);

    // Reset canvas
    state.canvas.new_canvas((int)w, (int)h);
    state.canvas.fps = fps;
    state.canvas.frames.clear();
    state.canvas.frames.reserve(frame_count);

    // FRAMES
    for (int fi = 0; fi < (int)frame_count; fi++) {
        uint16_t dur         = 100;
        uint16_t layer_count = 0;
        fread(&dur,         sizeof(uint16_t), 1, f);
        fread(&layer_count, sizeof(uint16_t), 1, f);

        Frame frame;
        frame.duration_ms = dur;
        frame.layers.reserve(layer_count);

        for (int li = 0; li < (int)layer_count; li++) {
            Layer layer;
            layer.canvas = Canvas((int)w, (int)h);

            read_str(f, layer.name);

            uint8_t vis = 1, lock = 0, blend = 0;
            float   op  = 1.0f;
            fread(&vis,   1,             1, f);
            fread(&lock,  1,             1, f);
            fread(&op,    sizeof(float), 1, f);
            fread(&blend, 1,             1, f);
            layer.visible    = (vis  != 0);
            layer.locked     = (lock != 0);
            layer.opacity    = op;
            layer.blend_mode = blend;
            fread(layer.canvas.pixels.data(), sizeof(uint32_t), (size_t)(w * h), f);
            frame.layers.push_back(std::move(layer));
        }

        state.canvas.frames.push_back(std::move(frame));
    }

    fclose(f);

    state.canvas.active_frame = 0;
    state.canvas.active_layer = 0;
    state.canvas.needs_center = true;
    state.canvas.rebuild_composite();

    Log("pixc_io::load: loaded \"%s\" (%d frames, %dx%d)", path.c_str(), (int)frame_count, (int)w, (int)h);
    return true;
}
