#include "test_util.h"
#include "app_state.h"

static void add_doc(AppState& app, int w, int h) {
    app.docs.emplace_back();
    app.docs.back().canvas.new_canvas(w, h);
}

// Undo on doc 0 must not revert pixels on doc 1.
static bool test_undo_does_not_affect_other_doc() {
    AppState app;
    app.canvas().new_canvas(4, 4);
    add_doc(app, 4, 4);

    app.active_doc = 0;
    app.canvas().push_snapshot();
    app.canvas().active().set(0, 0, RED);

    app.active_doc = 1;
    app.canvas().push_snapshot();
    app.canvas().active().set(0, 0, GREEN);

    app.active_doc = 0;
    app.canvas().undo();
    if (app.canvas().active().get(0, 0) != CLEAR) return false;

    app.active_doc = 1;
    return app.canvas().active().get(0, 0) == GREEN;
}

// Building a redo stack on doc 0 must leave doc 1's redo stack empty.
static bool test_redo_stack_independent() {
    AppState app;
    app.canvas().new_canvas(4, 4);
    add_doc(app, 4, 4);

    app.active_doc = 0;
    app.canvas().push_snapshot();
    app.canvas().active().set(0, 0, RED);
    app.canvas().undo();

    app.active_doc = 1;
    return app.canvas().redo_stack.empty();
}

// push_snapshot() on doc 0 sets unsaved_changes on doc 0; doc 1 stays clean.
static bool test_unsaved_changes_independent() {
    AppState app;
    app.canvas().new_canvas(4, 4);
    add_doc(app, 4, 4);

    app.active_doc = 0;
    app.canvas().push_snapshot();
    if (!app.canvas().unsaved_changes) return false;

    app.active_doc = 1;
    return !app.canvas().unsaved_changes;
}

// app.canvas() must resolve to the correct document after switching active_doc.
static bool test_active_doc_switch_returns_correct_canvas() {
    AppState app;
    app.canvas().new_canvas(4, 4);
    add_doc(app, 4, 4);

    app.active_doc = 0;
    app.canvas().active().set(1, 1, RED);

    app.active_doc = 1;
    app.canvas().active().set(1, 1, BLUE);

    app.active_doc = 0;
    if (app.canvas().active().get(1, 1) != RED) return false;

    app.active_doc = 1;
    return app.canvas().active().get(1, 1) == BLUE;
}

int main() {
    TestRunner t;
    t.run("undo_does_not_affect_other_doc",           test_undo_does_not_affect_other_doc);
    t.run("redo_stack_independent",                   test_redo_stack_independent);
    t.run("unsaved_changes_independent",              test_unsaved_changes_independent);
    t.run("active_doc_switch_returns_correct_canvas", test_active_doc_switch_returns_correct_canvas);
    return t.result();
}
