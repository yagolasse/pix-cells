#include <cstdio>
#include <filesystem>
#include "test_util.h"
#include "app_state.h"
#include "canvas.h"
#include "png_io.h"

static bool test_save_load_roundtrip() {
    const std::string path = tmp_path("test_png_io_rt.png");

    Canvas src(4, 4);
    src.set(0, 0, RED);
    src.set(3, 0, GREEN);
    src.set(0, 3, BLUE);
    src.set(2, 1, WHITE);

    if (!png_io::save(src, path)) { printf("  save failed\n"); return false; }

    Canvas dst;
    if (!png_io::load(dst, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (dst.width != 4 || dst.height != 4) {
        printf("  dimensions: got %dx%d want 4x4\n", dst.width, dst.height);
        return false;
    }
    if (dst.get(0, 0) != RED) {
        uint32_t p = dst.get(0, 0);
        printf("  (0,0): got R=%d G=%d B=%d A=%d want RED\n", cR(p), cG(p), cB(p), cA(p));
        return false;
    }
    if (dst.get(3, 0) != GREEN) {
        uint32_t p = dst.get(3, 0);
        printf("  (3,0): got R=%d G=%d B=%d A=%d want GREEN\n", cR(p), cG(p), cB(p), cA(p));
        return false;
    }
    if (dst.get(0, 3) != BLUE) {
        uint32_t p = dst.get(0, 3);
        printf("  (0,3): got R=%d G=%d B=%d A=%d want BLUE\n", cR(p), cG(p), cB(p), cA(p));
        return false;
    }
    if (dst.get(2, 1) != WHITE) {
        uint32_t p = dst.get(2, 1);
        printf("  (2,1): got R=%d G=%d B=%d A=%d want WHITE\n", cR(p), cG(p), cB(p), cA(p));
        return false;
    }
    return true;
}

static bool test_load_sets_dimensions() {
    const std::string path = tmp_path("test_png_io_dims.png");

    Canvas src(8, 3);
    src.fill(RED);

    if (!png_io::save(src, path)) { printf("  save failed\n"); return false; }

    Canvas dst;
    if (!png_io::load(dst, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (dst.width != 8 || dst.height != 3) {
        printf("  dimensions: got %dx%d want 8x3\n", dst.width, dst.height);
        return false;
    }
    return true;
}

static bool test_load_nonexistent() {
    Canvas c;
    bool result = png_io::load(c, "nonexistent_file_xyz.png");
    if (result) { printf("  should have returned false for nonexistent file\n"); return false; }
    return true;
}

static void make_frame(CanvasState& cs, int frame_idx, uint32_t fill_color) {
    cs.frames[frame_idx].layers[0].canvas.fill(fill_color);
    cs.frames[frame_idx].layers[0].visible = true;
    cs.frames[frame_idx].layers[0].opacity = 1.0f;
}

static void add_frame(CanvasState& cs, uint32_t fill_color) {
    Frame fr;
    Layer l;
    l.canvas = Canvas(cs.width(), cs.height());
    l.canvas.fill(fill_color);
    l.visible = true;
    l.opacity = 1.0f;
    fr.layers.push_back(std::move(l));
    cs.frames.push_back(std::move(fr));
}

static bool test_save_sprite_sheet_horizontal() {
    const std::string path = tmp_path("test_png_io_hsheet.png");

    CanvasState cs;
    cs.new_canvas(4, 4);
    make_frame(cs, 0, RED);
    add_frame(cs, BLUE);

    if (!png_io::save_sprite_sheet(cs, path, png_io::SheetLayout::Horizontal, 0)) {
        printf("  save_sprite_sheet failed\n"); return false;
    }

    Canvas result;
    if (!png_io::load(result, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (result.width != 8 || result.height != 4) {
        printf("  dimensions: got %dx%d want 8x4\n", result.width, result.height);
        return false;
    }
    uint32_t p00 = result.get(0, 0);
    if (cR(p00) != 255 || cG(p00) != 0 || cB(p00) != 0) {
        printf("  (0,0): got R=%d G=%d B=%d want RED\n", cR(p00), cG(p00), cB(p00));
        return false;
    }
    uint32_t p40 = result.get(4, 0);
    if (cR(p40) != 0 || cG(p40) != 0 || cB(p40) != 255) {
        printf("  (4,0): got R=%d G=%d B=%d want BLUE\n", cR(p40), cG(p40), cB(p40));
        return false;
    }
    return true;
}

static bool test_save_sprite_sheet_vertical() {
    const std::string path = tmp_path("test_png_io_vsheet.png");

    CanvasState cs;
    cs.new_canvas(4, 4);
    make_frame(cs, 0, RED);
    add_frame(cs, BLUE);

    if (!png_io::save_sprite_sheet(cs, path, png_io::SheetLayout::Vertical, 0)) {
        printf("  save_sprite_sheet failed\n"); return false;
    }

    Canvas result;
    if (!png_io::load(result, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (result.width != 4 || result.height != 8) {
        printf("  dimensions: got %dx%d want 4x8\n", result.width, result.height);
        return false;
    }
    uint32_t p00 = result.get(0, 0);
    if (cR(p00) != 255 || cG(p00) != 0 || cB(p00) != 0) {
        printf("  (0,0): got R=%d G=%d B=%d want RED\n", cR(p00), cG(p00), cB(p00));
        return false;
    }
    uint32_t p04 = result.get(0, 4);
    if (cR(p04) != 0 || cG(p04) != 0 || cB(p04) != 255) {
        printf("  (0,4): got R=%d G=%d B=%d want BLUE\n", cR(p04), cG(p04), cB(p04));
        return false;
    }
    return true;
}

static bool test_save_sprite_sheet_grid() {
    const std::string path = tmp_path("test_png_io_gsheet.png");

    CanvasState cs;
    cs.new_canvas(4, 4);
    make_frame(cs, 0, RED);
    add_frame(cs, GREEN);
    add_frame(cs, BLUE);
    add_frame(cs, WHITE);

    if (!png_io::save_sprite_sheet(cs, path, png_io::SheetLayout::Grid, 2)) {
        printf("  save_sprite_sheet failed\n"); return false;
    }

    Canvas result;
    if (!png_io::load(result, path)) { printf("  load failed\n"); return false; }
    std::filesystem::remove(path);

    if (result.width != 8 || result.height != 8) {
        printf("  dimensions: got %dx%d want 8x8\n", result.width, result.height);
        return false;
    }
    // top-left quadrant: RED
    uint32_t tl = result.get(0, 0);
    if (cR(tl) != 255 || cG(tl) != 0 || cB(tl) != 0) {
        printf("  top-left: got R=%d G=%d B=%d want RED\n", cR(tl), cG(tl), cB(tl));
        return false;
    }
    // top-right quadrant: GREEN
    uint32_t tr = result.get(4, 0);
    if (cR(tr) != 0 || cG(tr) != 255 || cB(tr) != 0) {
        printf("  top-right: got R=%d G=%d B=%d want GREEN\n", cR(tr), cG(tr), cB(tr));
        return false;
    }
    // bottom-left quadrant: BLUE
    uint32_t bl = result.get(0, 4);
    if (cR(bl) != 0 || cG(bl) != 0 || cB(bl) != 255) {
        printf("  bottom-left: got R=%d G=%d B=%d want BLUE\n", cR(bl), cG(bl), cB(bl));
        return false;
    }
    // bottom-right quadrant: WHITE
    uint32_t br = result.get(4, 4);
    if (cR(br) != 255 || cG(br) != 255 || cB(br) != 255) {
        printf("  bottom-right: got R=%d G=%d B=%d want WHITE\n", cR(br), cG(br), cB(br));
        return false;
    }
    return true;
}

int main() {
    TestRunner t;
    t.run("save_load_roundtrip",          test_save_load_roundtrip);
    t.run("load_sets_dimensions",         test_load_sets_dimensions);
    t.run("load_nonexistent",             test_load_nonexistent);
    t.run("sprite_sheet_horizontal",      test_save_sprite_sheet_horizontal);
    t.run("sprite_sheet_vertical",        test_save_sprite_sheet_vertical);
    t.run("sprite_sheet_grid",            test_save_sprite_sheet_grid);
    return t.result();
}
