#include "app_state.h"
#include "log.h"

void lift_selection(CanvasState& cs, SelectionState& sel) {
    cs.push_snapshot();
    int sw = sel.x1 - sel.x0 + 1;
    int sh = sel.y1 - sel.y0 + 1;
    sel.float_pixels.resize(sw * sh);
    sel.float_w      = sw;
    sel.float_h      = sh;
    sel.float_x      = sel.x0;
    sel.float_y      = sel.y0;
    sel.float_orig_x = sel.x0;
    sel.float_orig_y = sel.y0;
    Canvas& c = cs.active();
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++)
            sel.float_pixels[y * sw + x] = c.get(sel.x0 + x, sel.y0 + y);
    if (!sel.mask.empty()) {
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                if (!sel.mask[y * sw + x])
                    sel.float_pixels[y * sw + x] = 0x00000000;
    }
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++)
            if (sel.mask.empty() || sel.mask[y * sw + x])
                c.set(sel.x0 + x, sel.y0 + y, 0x00000000);
    sel.floating = true;
    cs.rebuild_composite();
    Log("Lift selection: %dx%d", sw, sh);
}

void commit_floating(CanvasState& cs, SelectionState& sel) {
    Canvas& c = cs.active();
    for (int y = 0; y < sel.float_h; y++)
        for (int x = 0; x < sel.float_w; x++) {
            uint32_t pv = sel.float_pixels[y * sel.float_w + x];
            if (((pv >> 24) & 0xFF) > 0)
                c.set(sel.float_x + x, sel.float_y + y, pv);
        }
    sel.active   = false;
    sel.floating = false;
    sel.float_pixels.clear();
    sel.mask.clear();
    sel.sel_revision++;
    cs.rebuild_composite();
    Log("Commit float to (%d,%d)", sel.float_x, sel.float_y);
}
