#include "test_util.h"
#include "app_state.h"

// push_snapshot() saves current frames + active indices and clears the redo
// stack. undo()/redo() swap between the stacks and rebuild the composite.
// undo_stack is capped at MAX_HISTORY (50).

static bool test_undo_redo_pixels() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.push_snapshot();          // snapshot: pixel (0,0) is transparent
    cs.active().set(0, 0, RED);
    cs.undo();
    if (cs.active().get(0, 0) != CLEAR) return false;
    cs.redo();
    return cs.active().get(0, 0) == RED;
}

static bool test_undo_restores_indices() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    Frame fr; Layer l; l.canvas = Canvas(4, 4); fr.layers.push_back(std::move(l));
    cs.frames.push_back(std::move(fr)); // 2 frames
    cs.active_frame = 0;
    cs.push_snapshot();
    cs.active_frame = 1;
    cs.undo();
    return cs.active_frame == 0;
}

static bool test_push_clears_redo() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.push_snapshot();
    cs.active().set(0, 0, RED);
    cs.undo();                   // redo_stack now has 1 entry
    if (cs.redo_stack.empty()) return false;
    cs.push_snapshot();          // any new snapshot clears redo
    return cs.redo_stack.empty();
}

static bool test_undo_redo_empty_noop() {
    CanvasState cs;
    cs.new_canvas(4, 4);
    cs.undo(); // no snapshots — must not crash or change state
    cs.redo();
    return (int)cs.frames.size() == 1;
}

static bool test_max_history_cap() {
    CanvasState cs;
    cs.new_canvas(2, 2);
    for (int i = 0; i < CanvasState::MAX_HISTORY + 10; i++)
        cs.push_snapshot();
    return (int)cs.undo_stack.size() == CanvasState::MAX_HISTORY;
}

int main() {
    TestRunner t;
    t.run("undo_redo_pixels",     test_undo_redo_pixels);
    t.run("undo_restores_indices",test_undo_restores_indices);
    t.run("push_clears_redo",     test_push_clears_redo);
    t.run("undo_redo_empty_noop", test_undo_redo_empty_noop);
    t.run("max_history_cap",      test_max_history_cap);
    return t.result();
}
