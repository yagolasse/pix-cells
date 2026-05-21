#include "app_state.h"
#include "log.h"

// Composite src over dst with blend mode and pre-applied opacity — RGBA8, R in bits 0-7
static uint32_t blend_pixel(uint32_t dst, uint32_t src, uint8_t mode) {
    float sa = ((src >> 24) & 0xFF) / 255.0f;
    if (sa <= 0.0f) return dst;
    float sr = (src        & 0xFF) / 255.0f;
    float sg = ((src >>  8) & 0xFF) / 255.0f;
    float sb = ((src >> 16) & 0xFF) / 255.0f;
    float da = ((dst >> 24) & 0xFF) / 255.0f;
    float dr = (dst        & 0xFF) / 255.0f;
    float dg = ((dst >>  8) & 0xFF) / 255.0f;
    float db = ((dst >> 16) & 0xFF) / 255.0f;

    float br = sr, bg = sg, bb = sb;
    switch (mode) {
    case 1: br=sr*dr;                bg=sg*dg;                bb=sb*db;                break; // Multiply
    case 2: br=1-(1-sr)*(1-dr);      bg=1-(1-sg)*(1-dg);      bb=1-(1-sb)*(1-db);      break; // Screen
    case 3: br=dr<.5f?2*sr*dr:1-2*(1-sr)*(1-dr);                                             // Overlay
            bg=dg<.5f?2*sg*dg:1-2*(1-sg)*(1-dg);
            bb=db<.5f?2*sb*db:1-2*(1-sb)*(1-db);              break;
    case 4: br=std::min(sr+dr,1.f);  bg=std::min(sg+dg,1.f);  bb=std::min(sb+db,1.f);  break; // Add
    default: break; // Normal
    }
    float oa  = sa + da * (1.0f - sa);
    if (oa == 0.0f) return 0;
    float or_ = (br*sa + dr*da*(1.0f-sa)) / oa;
    float og  = (bg*sa + dg*da*(1.0f-sa)) / oa;
    float ob  = (bb*sa + db*da*(1.0f-sa)) / oa;
    return ((uint32_t)(oa *255.0f)<<24)
         | ((uint32_t)(ob *255.0f)<<16)
         | ((uint32_t)(og *255.0f)<< 8)
         |  (uint32_t)(or_*255.0f);
}

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
    composite_frame(active_frame, composite);
    dirty = true;
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
}

void CanvasState::undo() {
    if (undo_stack.empty()) return;
    redo_stack.push_back({ frames, active_frame, active_layer });
    HistoryState snap = std::move(undo_stack.back());
    undo_stack.pop_back();
    frames       = std::move(snap.frames);
    active_frame = snap.active_frame;
    active_layer = snap.active_layer;
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
    composite.resize(w * h, 0x00000000);
    rebuild_composite();
    Log("New canvas: %dx%d", w, h);
}
