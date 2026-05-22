#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include "test_util.h"
#include "app_state.h"
#include "palette_io.h"

// Float channel → 0-255 int for comparison.
static int ch(float v) { return (int)(v * 255.0f + 0.5f); }

// ── HEX ──────────────────────────────────────────────────────────────────────

static bool test_hex_roundtrip() {
    const std::string path = tmp_path("test_pal_hex_rt.hex");

    PaletteState orig;
    orig.palette_name = "test-palette";
    orig.swatches = {
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    };

    if (!palette_io::save_hex(orig, path)) { printf("  save failed\n"); return false; }

    PaletteState loaded;
    if (!palette_io::load_hex(loaded, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if ((int)loaded.swatches.size() != 4) {
        printf("  count: got %d want 4\n", (int)loaded.swatches.size());
        return false;
    }
    if (loaded.palette_name != "test-palette") {
        printf("  name: got \"%s\"\n", loaded.palette_name.c_str());
        return false;
    }
    if (loaded.selected_swatch != -1) {
        printf("  selected_swatch not reset: got %d\n", loaded.selected_swatch);
        return false;
    }
    // Spot-check red and blue.
    if (ch(loaded.swatches[0].x) != 255 || ch(loaded.swatches[0].y) != 0 || ch(loaded.swatches[0].z) != 0) {
        printf("  swatch[0]: got (%d,%d,%d) want (255,0,0)\n",
               ch(loaded.swatches[0].x), ch(loaded.swatches[0].y), ch(loaded.swatches[0].z));
        return false;
    }
    if (ch(loaded.swatches[2].z) != 255 || ch(loaded.swatches[2].x) != 0) {
        printf("  swatch[2]: got (%d,%d,%d) want (0,0,255)\n",
               ch(loaded.swatches[2].x), ch(loaded.swatches[2].y), ch(loaded.swatches[2].z));
        return false;
    }
    return true;
}

static bool test_hex_name_fallback() {
    // No "; comment" line → palette_name should come from the filename stem.
    const std::string path = tmp_path("my-palette.hex");
    { std::ofstream f(path); f << "#FF0000\n#00FF00\n"; }

    PaletteState pal;
    pal.palette_name = "old-name";
    if (!palette_io::load_hex(pal, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (pal.palette_name != "my-palette") {
        printf("  name fallback: got \"%s\" want \"my-palette\"\n", pal.palette_name.c_str());
        return false;
    }
    return true;
}

static bool test_hex_hash_and_bare() {
    // Both "#RRGGBB" and bare "RRGGBB" should parse.
    const std::string path = tmp_path("test_pal_hex_mix.hex");
    {
        std::ofstream f(path);
        f << "#FF0000\n";  // prefixed
        f << "00FF00\n";   // bare
        f << "0000FF\n";   // bare
    }

    PaletteState pal;
    if (!palette_io::load_hex(pal, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if ((int)pal.swatches.size() != 3) {
        printf("  count: got %d want 3\n", (int)pal.swatches.size());
        return false;
    }
    if (ch(pal.swatches[1].y) != 255 || ch(pal.swatches[1].x) != 0) {
        printf("  swatch[1] (bare green): got (%d,%d,%d)\n",
               ch(pal.swatches[1].x), ch(pal.swatches[1].y), ch(pal.swatches[1].z));
        return false;
    }
    return true;
}

static bool test_hex_malformed_skipped() {
    // Junk lines and invalid hex must be silently skipped.
    const std::string path = tmp_path("test_pal_hex_junk.hex");
    {
        std::ofstream f(path);
        f << "; comment\n";
        f << "// also comment\n";
        f << "not-a-color\n";
        f << "#GGGGGG\n";   // invalid hex chars
        f << "#12345G\n";   // invalid last char
        f << "FF00FF\n";    // valid
        f << "\n";
        f << "AABBCC\n";    // valid
    }

    PaletteState pal;
    if (!palette_io::load_hex(pal, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if ((int)pal.swatches.size() != 2) {
        printf("  count: got %d want 2\n", (int)pal.swatches.size());
        return false;
    }
    return true;
}

static bool test_hex_empty_returns_false() {
    // Comments only → no colors → load must fail without touching palette.
    const std::string path = tmp_path("test_pal_hex_empty.hex");
    { std::ofstream f(path); f << "; no colors here\n// nothing\n"; }

    PaletteState pal;
    pal.palette_name = "unchanged";
    bool result = palette_io::load_hex(pal, path);
    std::filesystem::remove(path);

    if (result) { printf("  should have returned false\n"); return false; }
    if (pal.palette_name != "unchanged") {
        printf("  palette_name modified despite failure\n"); return false;
    }
    return true;
}

// ── GPL ──────────────────────────────────────────────────────────────────────

static bool test_gpl_roundtrip() {
    const std::string path = tmp_path("test_pal_gpl_rt.gpl");

    PaletteState orig;
    orig.palette_name = "lospec500";
    orig.swatches = {
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    };

    if (!palette_io::save_gpl(orig, path)) { printf("  save failed\n"); return false; }

    PaletteState loaded;
    if (!palette_io::load_gpl(loaded, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if ((int)loaded.swatches.size() != 3) {
        printf("  count: got %d want 3\n", (int)loaded.swatches.size());
        return false;
    }
    if (loaded.palette_name != "lospec500") {
        printf("  name: got \"%s\"\n", loaded.palette_name.c_str());
        return false;
    }
    if (loaded.selected_swatch != -1) {
        printf("  selected_swatch not reset: got %d\n", loaded.selected_swatch);
        return false;
    }
    if (ch(loaded.swatches[2].z) != 255 || ch(loaded.swatches[2].x) != 0) {
        printf("  swatch[2] (blue): got (%d,%d,%d)\n",
               ch(loaded.swatches[2].x), ch(loaded.swatches[2].y), ch(loaded.swatches[2].z));
        return false;
    }
    return true;
}

static bool test_gpl_name_fallback() {
    // No "Name:" line → palette_name from filename stem.
    const std::string path = tmp_path("my-gpl-palette.gpl");
    {
        std::ofstream f(path);
        f << "GIMP Palette\n";
        f << "Columns: 4\n";
        f << "#\n";
        f << "255   0   0\tRed\n";
        f << "  0 255   0\tGreen\n";
    }

    PaletteState pal;
    pal.palette_name = "old-name";
    if (!palette_io::load_gpl(pal, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (pal.palette_name != "my-gpl-palette") {
        printf("  name fallback: got \"%s\" want \"my-gpl-palette\"\n", pal.palette_name.c_str());
        return false;
    }
    if ((int)pal.swatches.size() != 2) {
        printf("  count: got %d want 2\n", (int)pal.swatches.size());
        return false;
    }
    return true;
}

static bool test_gpl_malformed_skipped() {
    // Junk and out-of-range lines handled correctly.
    const std::string path = tmp_path("test_pal_gpl_junk.gpl");
    {
        std::ofstream f(path);
        f << "GIMP Palette\n";
        f << "Name: junk-test\n";
        f << "#\n";
        f << "255   0   0\tRed\n";   // valid
        f << "not a color\n";        // junk — skipped (sscanf returns < 3)
        f << "  0 255   0\tGreen\n"; // valid
        f << "999 999 999\tOOB\n";   // out-of-range — clamped to 255
        f << "\n";                    // blank — skipped
    }

    PaletteState pal;
    if (!palette_io::load_gpl(pal, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if ((int)pal.swatches.size() != 3) {
        printf("  count: got %d want 3\n", (int)pal.swatches.size());
        return false;
    }
    if (ch(pal.swatches[2].x) != 255 || ch(pal.swatches[2].y) != 255 || ch(pal.swatches[2].z) != 255) {
        printf("  oob clamped swatch: got (%d,%d,%d) want (255,255,255)\n",
               ch(pal.swatches[2].x), ch(pal.swatches[2].y), ch(pal.swatches[2].z));
        return false;
    }
    return true;
}

static bool test_gpl_bad_magic() {
    const std::string path = tmp_path("test_pal_gpl_badmagic.gpl");
    { std::ofstream f(path); f << "NOT A GPL FILE\n#\n255 0 0\tRed\n"; }

    PaletteState pal;
    bool result = palette_io::load_gpl(pal, path);
    std::filesystem::remove(path);

    if (result) { printf("  should have returned false for bad magic\n"); return false; }
    return true;
}

static bool test_gpl_no_separator_returns_false() {
    // Missing '#' separator → load must fail.
    const std::string path = tmp_path("test_pal_gpl_nosep.gpl");
    {
        std::ofstream f(path);
        f << "GIMP Palette\n";
        f << "Name: no-sep\n";
        f << "255 0 0\tRed\n";  // color-like line before separator — must not be read
    }

    PaletteState pal;
    bool result = palette_io::load_gpl(pal, path);
    std::filesystem::remove(path);

    if (result) { printf("  should have returned false with no '#' separator\n"); return false; }
    return true;
}

int main() {
    TestRunner t;
    t.run("hex_roundtrip",              test_hex_roundtrip);
    t.run("hex_name_fallback",          test_hex_name_fallback);
    t.run("hex_hash_and_bare",          test_hex_hash_and_bare);
    t.run("hex_malformed_skipped",      test_hex_malformed_skipped);
    t.run("hex_empty_returns_false",    test_hex_empty_returns_false);
    t.run("gpl_roundtrip",              test_gpl_roundtrip);
    t.run("gpl_name_fallback",          test_gpl_name_fallback);
    t.run("gpl_malformed_skipped",      test_gpl_malformed_skipped);
    t.run("gpl_bad_magic",              test_gpl_bad_magic);
    t.run("gpl_no_separator_returns_false", test_gpl_no_separator_returns_false);
    return t.result();
}
