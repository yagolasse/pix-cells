#include "app_state.h"
#include "blend.h"
#include "log.h"
#include "raster.h"

static uint64_t s_frame_rev_gen = 0; // monotonically increasing; assigned to frame_revisions entries

CanvasState::CanvasState() {
    Frame f;
    Layer l;
    l.name = "Layer 1";
    f.layers.push_back(std::move(l));
    frames.push_back(std::move(f));
    composite.resize(64 * 64, 0x00000000);
    rebuild_composite();
}

int CanvasState::width() const {
    return frames.empty() || frames[0].layers.empty() ? 0 : frames[0].layers[0].canvas.width;
}

int CanvasState::height() const {
    return frames.empty() || frames[0].layers.empty() ? 0 : frames[0].layers[0].canvas.height;
}

Canvas& CanvasState::active() {
    return frames[active_frame].layers[active_layer].canvas;
}

void CanvasState::composite_frame(int frame_idx, std::vector<uint32_t>& out) const {
    int sz = width() * height();
    out.assign(sz, 0x00000000);
    if (frame_idx < 0 || frame_idx >= (int)frames.size()) return;
    for (const auto& layer : frames[frame_idx].layers) {
        if (!layer.visible) continue;
        for (int i = 0; i < sz; i++) {
            uint32_t src = layer.canvas.pixels[i];
            if (layer.opacity < 1.0f) {
                uint8_t a = (uint8_t)(((src >> 24) & 0xFF) * layer.opacity);
                src = (src & 0x00FFFFFF) | ((uint32_t)a << 24);
            }
            out[i] = blend_pixel(out[i], src, layer.blend_mode);
        }
    }
}

void CanvasState::rebuild_composite() {
    // Sync frame_revisions with current frame count; give new frames unique IDs
    while (frame_revisions.size() < frames.size())
        frame_revisions.push_back(++s_frame_rev_gen);
    frame_revisions.resize(frames.size()); // truncate if frames were deleted
    composite_frame(active_frame, composite);
    dirty = true;
    if (active_frame < (int)frame_revisions.size())
        frame_revisions[active_frame] = ++s_frame_rev_gen;
}

void CanvasState::duplicate_frame(int idx) {
    if (idx < 0 || idx >= (int)frames.size()) return;
    push_snapshot();
    frames.insert(frames.begin() + idx + 1, frames[idx]);
    // Extend any tag whose range covers or starts after the insertion point.
    for (auto& t : tags) {
        if (t.end >= idx + 1)   t.end++;
        if (t.start >= idx + 1) t.start++;
    }
    active_frame = idx + 1;
    rebuild_composite();
    Log("Timeline: duplicated frame %d", idx + 1);
}

void CanvasState::delete_frame(int idx) {
    if (idx < 0 || idx >= (int)frames.size()) return;
    if ((int)frames.size() <= 1) {
        Log("Timeline: cannot delete the last frame");
        return;
    }
    push_snapshot();
    frames.erase(frames.begin() + idx);
    int last = (int)frames.size() - 1;
    for (auto& t : tags) {
        if (t.start > idx) t.start--;
        if (t.end >= idx)  t.end--;
        if (t.start < 0)   t.start = 0;
        if (t.end < t.start) t.end = t.start;
        if (t.end > last)  t.end = last;
        if (t.start > last) t.start = last;
    }
    if (active_frame > idx)   active_frame--;
    if (active_frame > last)  active_frame = last;
    rebuild_composite();
    Log("Timeline: deleted frame %d", idx + 1);
}

void CanvasState::push_snapshot() {
    undo_stack.push_back({ frames, active_frame, active_layer });
    if ((int)undo_stack.size() > MAX_HISTORY) undo_stack.pop_front();
    redo_stack.clear();
    unsaved_changes = true;
}

void CanvasState::undo() {
    if (undo_stack.empty()) return;
    redo_stack.push_back({ frames, active_frame, active_layer });
    HistoryState snap = std::move(undo_stack.back());
    undo_stack.pop_back();
    frames       = std::move(snap.frames);
    active_frame = snap.active_frame;
    active_layer = snap.active_layer;
    unsaved_changes = true;
    rebuild_composite();
    Log("Undo (%d steps remain)", (int)undo_stack.size());
}

void CanvasState::redo() {
    if (redo_stack.empty()) return;
    undo_stack.push_back({ frames, active_frame, active_layer });
    HistoryState snap = std::move(redo_stack.back());
    redo_stack.pop_back();
    frames       = std::move(snap.frames);
    active_frame = snap.active_frame;
    active_layer = snap.active_layer;
    unsaved_changes = true;
    rebuild_composite();
    Log("Redo (%d steps remain)", (int)redo_stack.size());
}

void CanvasState::new_canvas(int w, int h) {
    frames.clear();
    Frame f;
    Layer l;
    l.name   = "Layer 1";
    l.canvas = Canvas(w, h);
    f.layers.push_back(std::move(l));
    frames.push_back(std::move(f));
    active_frame = 0;
    active_layer = 0;
    pan          = { 0.0f, 0.0f };
    needs_center = true;
    undo_stack.clear();
    redo_stack.clear();
    unsaved_changes = false;
    composite.resize(w * h, 0x00000000);
    rebuild_composite();
    Log("New canvas: %dx%d", w, h);
}

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
    cs.rebuild_composite();
    Log("Commit float to (%d,%d)", sel.float_x, sel.float_y);
}
