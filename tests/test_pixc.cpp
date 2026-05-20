#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "app_state.h"
#include "pixc_io.h"

static std::string tmp_path(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

// ---------------------------------------------------------------------------
// Test 1 — minimal round-trip
// ---------------------------------------------------------------------------
static bool test_minimal_roundtrip() {
    const std::string path = tmp_path("test_pixc_minimal.pixc");

    // Build state
    AppState state;
    state.canvas.frames[0].layers[0].canvas.set(0, 0, 0xFF0000FFu);
    state.canvas.fps = 24.0f;
    state.palette.palette_name = "test";

    if (!pixc_io::save(state, path)) {
        printf("  save failed\n");
        return false;
    }

    AppState loaded;
    if (!pixc_io::load(loaded, path)) {
        printf("  load failed\n");
        return false;
    }

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

    // Configure first frame
    state.canvas.frames[0].duration_ms = 100;
    state.canvas.frames[0].layers[0].opacity = 0.5f;
    state.canvas.frames[0].layers[0].name = "background";
    state.canvas.frames[0].layers[0].canvas.set(1, 1, 0x00FF00FFu);

    // Add a second frame
    Frame frame2;
    frame2.duration_ms = 200;
    Layer layer2;
    layer2.canvas = Canvas(state.canvas.width(), state.canvas.height());
    layer2.canvas.set(2, 2, 0x0000FFFFu);
    layer2.name = "frame2layer";
    frame2.layers.push_back(std::move(layer2));
    state.canvas.frames.push_back(std::move(frame2));

    if (!pixc_io::save(state, path)) {
        printf("  save failed\n");
        return false;
    }

    AppState loaded;
    if (!pixc_io::load(loaded, path)) {
        printf("  load failed\n");
        return false;
    }

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
    if (result) {
        printf("  load should have returned false for bad magic\n");
        std::filesystem::remove(path);
        return false;
    }

    std::filesystem::remove(path);
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    int failed = 0;
    auto run = [&](const char* name, bool(*fn)()) {
        if (fn()) printf("PASS: %s\n", name);
        else     { printf("FAIL: %s\n", name); failed++; }
    };
    run("minimal_roundtrip", test_minimal_roundtrip);
    run("multiframe",        test_multiframe);
    run("bad_magic",         test_bad_magic);
    return failed ? 1 : 0;
}
