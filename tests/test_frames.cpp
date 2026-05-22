#include "test_util.h"
#include "app_state.h"

// duplicate_frame(idx): inserts a copy after idx, shifts tags at/after the
//   insertion point, sets active_frame = idx+1.
// delete_frame(idx): no-op if only one frame; else erases, decrements/clamps
//   tag ranges and active_frame.

static void add_frames(CanvasState& cs, int n) {
    for (int i = 0; i < n; i++) {
        Frame fr;
        Layer l;
        l.canvas = Canvas(cs.width(), cs.height());
        fr.layers.push_back(std::move(l));
        cs.frames.push_back(std::move(fr));
    }
}

static bool test_duplicate_inserts_after() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    add_frames(cs, 1); // 2 frames now
    cs.duplicate_frame(0);
    return (int)cs.frames.size() == 3 && cs.active_frame == 1;
}

static bool test_duplicate_shifts_tags() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    add_frames(cs, 1); // 2 frames
    AnimTag t; t.name = "a"; t.start = 1; t.end = 1;
    cs.tags.push_back(t);
    cs.duplicate_frame(0); // insert after 0 → tag at index 1 shifts to 2
    return cs.tags[0].start == 2 && cs.tags[0].end == 2;
}

static bool test_delete_reduces_and_clamps_tags() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    add_frames(cs, 2); // 3 frames (0,1,2)
    AnimTag t; t.name = "a"; t.start = 2; t.end = 2;
    cs.tags.push_back(t);
    cs.delete_frame(0); // last becomes 1; tag {2,2} → {1,1}
    return (int)cs.frames.size() == 2 && cs.tags[0].start == 1 && cs.tags[0].end == 1;
}

static bool test_delete_last_frame_noop() {
    CanvasState cs;
    cs.new_canvas(4, 4); // single frame
    cs.delete_frame(0);
    return (int)cs.frames.size() == 1;
}

static bool test_delete_clamps_active() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    add_frames(cs, 2); // 3 frames
    cs.active_frame = 2;
    cs.delete_frame(2); // last becomes 1; active clamps to 1
    return (int)cs.frames.size() == 2 && cs.active_frame == 1;
}

int main() {
    TestRunner t;
    t.run("duplicate_inserts_after",        test_duplicate_inserts_after);
    t.run("duplicate_shifts_tags",          test_duplicate_shifts_tags);
    t.run("delete_reduces_and_clamps_tags", test_delete_reduces_and_clamps_tags);
    t.run("delete_last_frame_noop",         test_delete_last_frame_noop);
    t.run("delete_clamps_active",           test_delete_clamps_active);
    return t.result();
}
