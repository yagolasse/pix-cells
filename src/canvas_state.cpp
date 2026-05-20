#include "app_state.h"
#include "log.h"

// Porter-Duff "over" — RGBA8, R in bits 0-7
static uint32_t blend_over(uint32_t dst, uint32_t src) {
    float sa = ((src >> 24) & 0xFF) / 255.0f;
    if (sa <= 0.0f) return dst;
    if (sa >= 1.0f) return src;

    float da = ((dst >> 24) & 0xFF) / 255.0f;
    float sr = (src       & 0xFF) / 255.0f;
    float sg = ((src >> 8) & 0xFF) / 255.0f;
    float sb = ((src >>16) & 0xFF) / 255.0f;
    float dr = (dst       & 0xFF) / 255.0f;
    float dg = ((dst >> 8) & 0xFF) / 255.0f;
    float db = ((dst >>16) & 0xFF) / 255.0f;

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

CanvasState::CanvasState() {
    Layer l; l.name = "Layer 1";
    layers.push_back(std::move(l));
    composite.resize(64 * 64, 0x00000000);
    rebuild_composite();
}

int     CanvasState::width()  const { return layers.empty() ? 0 : layers[0].canvas.width; }
int     CanvasState::height() const { return layers.empty() ? 0 : layers[0].canvas.height; }
Canvas& CanvasState::active()       { return layers[active_layer].canvas; }

void CanvasState::rebuild_composite() {
    int sz = width() * height();
    composite.assign(sz, 0x00000000);
    for (const auto& layer : layers) {
        if (!layer.visible) continue;
        float op = layer.opacity;
        if (op <= 0.0f) continue;
        for (int i = 0; i < sz; i++) {
            uint32_t src = layer.canvas.pixels[i];
            if (op < 1.0f) {
                uint32_t a = (uint32_t)(((src >> 24) & 0xFF) * op + 0.5f);
                src = (src & 0x00FFFFFF) | (a << 24);
            }
            composite[i] = blend_over(composite[i], src);
        }
    }
    dirty = true;
}

void CanvasState::push_snapshot() {
    undo_stack.push_back(layers);
    if ((int)undo_stack.size() > MAX_HISTORY) undo_stack.pop_front();
    redo_stack.clear();
}

void CanvasState::undo() {
    if (undo_stack.empty()) return;
    redo_stack.push_back(layers);
    layers = std::move(undo_stack.back());
    undo_stack.pop_back();
    rebuild_composite();
    Log("Undo (%d steps remain)", (int)undo_stack.size());
}

void CanvasState::redo() {
    if (redo_stack.empty()) return;
    undo_stack.push_back(layers);
    layers = std::move(redo_stack.back());
    redo_stack.pop_back();
    rebuild_composite();
    Log("Redo (%d steps remain)", (int)redo_stack.size());
}

void CanvasState::new_canvas(int w, int h) {
    layers.clear();
    Layer l; l.name = "Layer 1"; l.canvas = Canvas(w, h);
    layers.push_back(std::move(l));
    active_layer = 0;
    pan          = { 0.0f, 0.0f };
    needs_center = true;
    undo_stack.clear();
    redo_stack.clear();
    composite.resize(w * h, 0x00000000);
    rebuild_composite();
    Log("New canvas: %dx%d", w, h);
}
