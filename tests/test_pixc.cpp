#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "test_util.h"
#include "app_state.h"
#include "pixc_io.h"

// ---------------------------------------------------------------------------
// Test 1 — minimal round-trip
// ---------------------------------------------------------------------------
static bool test_minimal_roundtrip() {
    const std::string path = tmp_path("test_pixc_minimal.pixc");

    AppState state;
    state.canvas.frames[0].layers[0].canvas.set(0, 0, 0xFF0000FFu);
    state.canvas.fps = 24.0f;
    state.palette.palette_name = "test";

    if (!pixc_io::save(state, path)) { printf("  save failed\n"); return false; }

    AppState loaded;
    if (!pixc_io::load(loaded, path)) { printf("  load failed\n"); return false; }

    if (loaded.canvas.fps != 24.0f) {
        printf("  fps mismatch: got %f\n", (double)loaded.canvas.fps);
        return false;
    }
    if ((int)loaded.canvas.frames.size() != 1) {
        printf("  frame count mismatch: got %d\n", (int)loaded.canvas.frames.size());
        return false;
    }
    if ((int)loaded.canvas.frames[0].layers.size() != 1) {
        printf("  layer count mismatch: got %d\n", (int)loaded.canvas.frames[0].layers.size());
        return false;
    }
    uint32_t pixel = loaded.canvas.frames[0].layers[0].canvas.get(0, 0);
    if (pixel != 0xFF0000FFu) {
        printf("  pixel mismatch: got 0x%08X\n", pixel);
        return false;
    }
    if (loaded.palette.palette_name != "test") {
        printf("  palette_name mismatch: got \"%s\"\n", loaded.palette.palette_name.c_str());
        return false;
    }

    std::filesystem::remove(path);
    return true;
}

// ---------------------------------------------------------------------------
// Test 2 — multiple frames and layers
// ---------------------------------------------------------------------------
static bool test_multiframe() {
    const std::string path = tmp_path("test_pixc_multiframe.pixc");

    AppState state;
    state.canvas.frames[0].duration_ms = 100;
    state.canvas.frames[0].layers[0].opacity = 0.5f;
    state.canvas.frames[0].layers[0].name = "background";
    state.canvas.frames[0].layers[0].canvas.set(1, 1, 0x00FF00FFu);

    Frame frame2;
    frame2.duration_ms = 200;
    Layer layer2;
    layer2.canvas = Canvas(state.canvas.width(), state.canvas.height());
    layer2.canvas.set(2, 2, 0x0000FFFFu);
    layer2.name = "frame2layer";
    frame2.layers.push_back(std::move(layer2));
    state.canvas.frames.push_back(std::move(frame2));

    if (!pixc_io::save(state, path)) { printf("  save failed\n"); return false; }

    AppState loaded;
    if (!pixc_io::load(loaded, path)) { printf("  load failed\n"); return false; }

    if ((int)loaded.canvas.frames.size() != 2) {
        printf("  frame count mismatch: got %d\n", (int)loaded.canvas.frames.size());
        return false;
    }
    if (loaded.canvas.frames[1].duration_ms != 200) {
        printf("  frame[1].duration_ms mismatch: got %d\n", (int)loaded.canvas.frames[1].duration_ms);
        return false;
    }
    if (loaded.canvas.frames[0].layers[0].opacity != 0.5f) {
        printf("  opacity mismatch: got %f\n", (double)loaded.canvas.frames[0].layers[0].opacity);
        return false;
    }
    if (loaded.canvas.frames[0].layers[0].name != "background") {
        printf("  layer name mismatch: got \"%s\"\n", loaded.canvas.frames[0].layers[0].name.c_str());
        return false;
    }
    uint32_t pixel_f2 = loaded.canvas.frames[1].layers[0].canvas.get(2, 2);
    if (pixel_f2 != 0x0000FFFFu) {
        printf("  frame2 pixel mismatch: got 0x%08X\n", pixel_f2);
        return false;
    }

    std::filesystem::remove(path);
    return true;
}

// ---------------------------------------------------------------------------
// Test 3 — bad magic rejected
// ---------------------------------------------------------------------------
static bool test_bad_magic() {
    const std::string path = tmp_path("test_pixc_badmagic.pixc");
    {
        std::ofstream out(path, std::ios::binary);
        const char junk[20] = { 'J','U','N','K',0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
        out.write(junk, sizeof(junk));
    }

    AppState state;
    bool result = pixc_io::load(state, path);
    std::filesystem::remove(path);
    if (result) { printf("  load should have returned false for bad magic\n"); return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Test 4 — animation tags round-trip
// ---------------------------------------------------------------------------
static bool test_tags_roundtrip() {
    const std::string path = tmp_path("test_pixc_tags.pixc");

    AppState state;
    // Give it 3 frames
    for (int i = 0; i < 2; i++) {
        Frame fr;
        Layer l;
        l.canvas = Canvas(state.canvas.width(), state.canvas.height());
        fr.layers.push_back(std::move(l));
        state.canvas.frames.push_back(std::move(fr));
    }
    AnimTag t;
    t.name  = "walk";
    t.start = 0;
    t.end   = 2;
    state.canvas.tags.push_back(t);
    state.canvas.active_tag = 0;

    if (!pixc_io::save(state, path)) { printf("  save failed\n"); return false; }

    AppState loaded;
    if (!pixc_io::load(loaded, path)) { printf("  load failed\n"); return false; }

    std::filesystem::remove(path);

    if ((int)loaded.canvas.tags.size() != 1) {
        printf("  tag count mismatch: got %d\n", (int)loaded.canvas.tags.size());
        return false;
    }
    const AnimTag& lt = loaded.canvas.tags[0];
    if (lt.name != "walk" || lt.start != 0 || lt.end != 2) {
        printf("  tag mismatch: \"%s\" %d-%d\n", lt.name.c_str(), lt.start, lt.end);
        return false;
    }
    if (loaded.canvas.active_tag != 0) {
        printf("  active_tag mismatch: got %d\n", loaded.canvas.active_tag);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 5 — out-of-range tag is clamped to the frame range on load
// ---------------------------------------------------------------------------
static bool test_tag_clamping() {
    const std::string path = tmp_path("test_pixc_tagclamp.pixc");

    AppState state; // single default frame; add one more → 2 frames, last index = 1
    {
        Frame fr;
        Layer l;
        l.canvas = Canvas(state.canvas.width(), state.canvas.height());
        fr.layers.push_back(std::move(l));
        state.canvas.frames.push_back(std::move(fr));
    }
    AnimTag t;
    t.name  = "oob";
    t.start = 0;
    t.end   = 5; // beyond last frame (1)
    state.canvas.tags.push_back(t);

    if (!pixc_io::save(state, path)) { printf("  save failed\n"); return false; }

    AppState loaded;
    if (!pixc_io::load(loaded, path)) { printf("  load failed\n"); return false; }

    std::filesystem::remove(path);

    if ((int)loaded.canvas.tags.size() != 1) {
        printf("  tag count mismatch: got %d\n", (int)loaded.canvas.tags.size());
        return false;
    }
    if (loaded.canvas.tags[0].end != 1) {
        printf("  tag end not clamped: got %d (want 1)\n", loaded.canvas.tags[0].end);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 6 — version 1 file (no TAGS block) loads cleanly with no tags
// ---------------------------------------------------------------------------
static bool test_v1_compat() {
    const std::string path = tmp_path("test_pixc_v1.pixc");

    AppState state;
    state.canvas.frames[0].layers[0].canvas.set(3, 3, 0xFF00FF00u);
    AnimTag t; t.name = "ignored"; t.start = 0; t.end = 0;
    state.canvas.tags.push_back(t); // present in the v2 file, must be ignored once downgraded

    if (!pixc_io::save(state, path)) { printf("  save failed\n"); return false; }

    // Patch the version field (2 bytes at offset 4) from 2 to 1.
    {
        std::fstream fp(path, std::ios::binary | std::ios::in | std::ios::out);
        fp.seekp(4, std::ios::beg);
        uint16_t v1 = 1;
        fp.write(reinterpret_cast<const char*>(&v1), sizeof(v1));
    }

    AppState loaded;
    bool ok = pixc_io::load(loaded, path);
    std::filesystem::remove(path);

    if (!ok) { printf("  v1 load failed\n"); return false; }
    if (!loaded.canvas.tags.empty()) {
        printf("  v1 should have no tags: got %d\n", (int)loaded.canvas.tags.size());
        return false;
    }
    if (loaded.canvas.active_tag != -1) {
        printf("  v1 active_tag should be -1: got %d\n", loaded.canvas.active_tag);
        return false;
    }
    if (loaded.canvas.frames[0].layers[0].canvas.get(3, 3) != 0xFF00FF00u) {
        printf("  v1 pixel data lost\n");
        return false;
    }
    return true;
}

int main() {
    TestRunner t;
    t.run("minimal_roundtrip", test_minimal_roundtrip);
    t.run("multiframe",        test_multiframe);
    t.run("bad_magic",         test_bad_magic);
    t.run("tags_roundtrip",    test_tags_roundtrip);
    t.run("tag_clamping",      test_tag_clamping);
    t.run("v1_compat",         test_v1_compat);
    return t.result();
}
