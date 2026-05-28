#include "pixc_io.h"
#include "app_state.h"
#include "log.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>

// ---------------------------------------------------------------------------
// Helper: write/read length-prefixed strings (uint8_t length, then bytes)
// ---------------------------------------------------------------------------

static bool write_str(FILE* f, const std::string& s) {
    auto len = (uint8_t)(s.size() > 255 ? 255 : s.size());
    if (fwrite(&len, 1, 1, f) != 1) return false;
    if (len > 0 && fwrite(s.data(), 1, len, f) != len) return false;
    return true;
}

static bool read_str(FILE* f, std::string& s) {
    uint8_t len = 0;
    if (fread(&len, 1, 1, f) != 1) return false;
    s.resize(len);
    return len == 0 || fread(s.data(), 1, len, f) == len;
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

    const CanvasState& cs = state.canvas();

    // HEADER
    const char magic[4] = { 'P', 'I', 'X', 'C' };
    fwrite(magic, 1, 4, f);

    uint16_t version     = 2;
    auto     w           = (uint16_t)cs.width();
    auto     h           = (uint16_t)cs.height();
    auto     frame_count = (uint16_t)cs.frames.size();
    float    fps         = cs.fps;

    fwrite(&version,     sizeof(uint16_t), 1, f);
    fwrite(&w,           sizeof(uint16_t), 1, f);
    fwrite(&h,           sizeof(uint16_t), 1, f);
    fwrite(&frame_count, sizeof(uint16_t), 1, f);
    fwrite(&fps,         sizeof(float),    1, f);

    // PALETTE
    const PaletteState& pal = state.palette();
    auto color_count = (uint16_t)pal.swatches.size();
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

    // TAGS (version 2+)
    auto tag_count = (uint16_t)cs.tags.size();
    fwrite(&tag_count, sizeof(uint16_t), 1, f);
    for (const auto& t : cs.tags) {
        write_str(f, t.name);
        uint16_t ts = (uint16_t)t.start;
        uint16_t te = (uint16_t)t.end;
        fwrite(&ts, sizeof(uint16_t), 1, f);
        fwrite(&te, sizeof(uint16_t), 1, f);
    }
    auto active_tag = (int16_t)cs.active_tag;
    fwrite(&active_tag, sizeof(int16_t), 1, f);

    fclose(f);
    Log("pixc_io::save: saved \"%s\" (%d frames, %d tags, %dx%d)", path.c_str(), (int)frame_count, (int)tag_count, (int)w, (int)h);
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

    // Checked fread: returns false if fewer than n items were read.
    auto frd = [f](void* ptr, size_t sz, size_t n) -> bool {
        return fread(ptr, sz, n, f) == n;
    };

    // HEADER
    char magic[4] = {};
    if (!frd(magic, 1, 4) || memcmp(magic, "PIXC", 4) != 0) {
        Log("pixc_io::load: bad magic in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }

    uint16_t version = 0;
    if (!frd(&version, sizeof(uint16_t), 1)) {
        Log("pixc_io::load: truncated header in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }
    if (version < 1 || version > 2) {
        Log("pixc_io::load: unsupported version %d in \"%s\"", (int)version, path.c_str());
        fclose(f);
        return false;
    }

    uint16_t w = 0, h = 0, frame_count = 0;
    float    fps = 12.0f;
    if (!frd(&w, sizeof(uint16_t), 1) || !frd(&h, sizeof(uint16_t), 1) ||
        !frd(&frame_count, sizeof(uint16_t), 1) || !frd(&fps, sizeof(float), 1)) {
        Log("pixc_io::load: truncated header in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }

    constexpr uint16_t kMaxDim = 4096;
    if (w == 0 || h == 0 || w > kMaxDim || h > kMaxDim || frame_count == 0) {
        Log("pixc_io::load: invalid dimensions or frame count (%dx%d, %d frames) in \"%s\"",
            (int)w, (int)h, (int)frame_count, path.c_str());
        fclose(f);
        return false;
    }

    // PALETTE
    uint16_t color_count = 0;
    if (!frd(&color_count, sizeof(uint16_t), 1)) {
        Log("pixc_io::load: truncated palette in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }
    state.palette().swatches.resize(color_count);
    for (auto& c : state.palette().swatches) {
        if (!frd(&c.x, sizeof(float), 1) || !frd(&c.y, sizeof(float), 1) ||
            !frd(&c.z, sizeof(float), 1) || !frd(&c.w, sizeof(float), 1)) {
            Log("pixc_io::load: truncated palette colors in \"%s\"", path.c_str());
            fclose(f);
            return false;
        }
    }
    if (!read_str(f, state.palette().palette_name)) {
        Log("pixc_io::load: truncated palette name in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }

    // Reset canvas
    state.canvas().new_canvas((int)w, (int)h);
    state.canvas().fps = fps;
    state.canvas().frames.clear();
    state.canvas().frames.reserve(frame_count);

    // FRAMES
    for (int fi = 0; fi < (int)frame_count; fi++) {
        uint16_t dur         = 100;
        uint16_t layer_count = 0;
        if (!frd(&dur, sizeof(uint16_t), 1) || !frd(&layer_count, sizeof(uint16_t), 1)) {
            Log("pixc_io::load: truncated frame header at frame %d in \"%s\"", fi, path.c_str());
            fclose(f);
            return false;
        }

        Frame frame;
        frame.duration_ms = dur;
        frame.layers.reserve(layer_count);

        for (int li = 0; li < (int)layer_count; li++) {
            Layer layer;
            layer.canvas = Canvas((int)w, (int)h);

            if (!read_str(f, layer.name)) {
                Log("pixc_io::load: truncated layer name at frame %d layer %d in \"%s\"", fi, li, path.c_str());
                fclose(f);
                return false;
            }

            uint8_t vis = 1, lock = 0, blend = 0;
            float   op  = 1.0f;
            if (!frd(&vis, 1, 1) || !frd(&lock, 1, 1) ||
                !frd(&op, sizeof(float), 1) || !frd(&blend, 1, 1)) {
                Log("pixc_io::load: truncated layer attributes at frame %d layer %d in \"%s\"", fi, li, path.c_str());
                fclose(f);
                return false;
            }
            layer.visible    = (vis  != 0);
            layer.locked     = (lock != 0);
            layer.opacity    = op;
            layer.blend_mode = blend;
            if (!frd(layer.canvas.pixels.data(), sizeof(uint32_t), (size_t)w * h)) {
                Log("pixc_io::load: truncated pixel data at frame %d layer %d in \"%s\"", fi, li, path.c_str());
                fclose(f);
                return false;
            }
            frame.layers.push_back(std::move(layer));
        }

        state.canvas().frames.push_back(std::move(frame));
    }

    // TAGS (version 2+)
    state.canvas().tags.clear();
    state.canvas().active_tag = -1;
    if (version >= 2) {
        uint16_t tag_count = 0;
        if (!frd(&tag_count, sizeof(uint16_t), 1)) {
            Log("pixc_io::load: truncated tags block in \"%s\"", path.c_str());
            fclose(f);
            return false;
        }
        state.canvas().tags.reserve(tag_count);
        int last = (int)frame_count - 1;
        for (int ti = 0; ti < (int)tag_count; ti++) {
            AnimTag t;
            uint16_t ts = 0, te = 0;
            if (!read_str(f, t.name) ||
                !frd(&ts, sizeof(uint16_t), 1) || !frd(&te, sizeof(uint16_t), 1)) {
                Log("pixc_io::load: truncated tag %d in \"%s\"", ti, path.c_str());
                fclose(f);
                return false;
            }
            t.start = std::clamp((int)ts, 0, last);
            t.end   = std::clamp((int)te, t.start, last);
            state.canvas().tags.push_back(std::move(t));
        }
        int16_t active_tag = -1;
        if (!frd(&active_tag, sizeof(int16_t), 1)) {
            Log("pixc_io::load: truncated active_tag in \"%s\"", path.c_str());
            fclose(f);
            return false;
        }
        int at = (int)active_tag;
        state.canvas().active_tag = (at >= 0 && at < (int)state.canvas().tags.size()) ? at : -1;
    }

    fclose(f);

    state.canvas().active_frame = 0;
    state.canvas().active_layer = 0;
    state.canvas().needs_center = true;
    state.canvas().rebuild_composite();

    Log("pixc_io::load: loaded \"%s\" (v%d, %d frames, %d tags, %dx%d)", path.c_str(), (int)version, (int)frame_count, (int)state.canvas().tags.size(), (int)w, (int)h);
    return true;
}
