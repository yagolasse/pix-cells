#include "input_handler.h"
#include "app_state.h"
#include "log.h"
#include "imgui.h"
#include <algorithm>

namespace input_handler {

static void fill_clipboard(SelectionState& sel, const Canvas& c) {
    int sw = sel.width(), sh = sel.height();
    sel.clipboard.resize(static_cast<size_t>(sw) * sh);
    sel.clipboard_w  = sw;
    sel.clipboard_h  = sh;
    sel.clipboard_ox = sel.x0;
    sel.clipboard_oy = sel.y0;
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++)
            sel.clipboard[y * sw + x] =
                sel.mask_selected(sel.x0 + x, sel.y0 + y)
                ? c.get(sel.x0 + x, sel.y0 + y)
                : 0x00000000u;
}

void handle_keyboard(AppState& app) {
    bool any_popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    if (!any_popup) {

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        app.canvas().undo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y))
        app.canvas().redo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W))
        app.close_active_doc_requested = true;

    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_B)) {
            app.tools.active_tool = tool::Brush;
            Log("Tool: Brush");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) {
            app.tools.active_tool = tool::Eraser;
            Log("Tool: Eraser");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            app.tools.active_tool = tool::Fill;
            Log("Tool: Fill");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_L)) {
            app.tools.active_tool = tool::Line;
            Log("Tool: Line");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            app.tools.active_tool = (app.tools.active_tool == tool::Rect) ? tool::FilledRect : tool::Rect;
            Log("Tool: %s", app.tools.active_tool == tool::Rect ? "Rect" : "Filled Rect");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_U)) {
            app.tools.active_tool = (app.tools.active_tool == tool::Circle) ? tool::FilledCircle : tool::Circle;
            Log("Tool: %s", app.tools.active_tool == tool::Circle ? "Circle" : "Filled Circle");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_M)) {
            app.tools.active_tool = tool::Move;
            Log("Tool: Move");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            app.tools.active_tool = tool::RectSelect;
            Log("Tool: Rect Select");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_I)) {
            app.tools.active_tool = tool::ColorPicker;
            Log("Tool: Color Picker");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) {
            app.tools.active_tool = tool::ColorSelect;
            Log("Tool: Color Select");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
            app.tools.brush_size = std::max(1, app.tools.brush_size - 1);
            Log("Brush size: %d", app.tools.brush_size);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
            app.tools.brush_size = std::min(32, app.tools.brush_size + 1);
            Log("Brush size: %d", app.tools.brush_size);
        }

        // Select all
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A)) {
            app.selection().active = true;
            app.selection().x0 = 0; app.selection().y0 = 0;
            app.selection().x1 = app.canvas().width() - 1;
            app.selection().y1 = app.canvas().height() - 1;
            app.selection().mask.clear();
            Log("Select All");
        }

        // Paste (works any time clipboard is non-empty)
        if (!app.selection().clipboard.empty() &&
            ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
            if (app.canvas().active_layer_locked()) {
                Log("Layer locked");
            } else {
                app.canvas().push_snapshot();
                Canvas& c = app.canvas().active();
                for (int y = 0; y < app.selection().clipboard_h; y++)
                    for (int x = 0; x < app.selection().clipboard_w; x++)
                        c.set(app.selection().clipboard_ox + x, app.selection().clipboard_oy + y,
                              app.selection().clipboard[y * app.selection().clipboard_w + x]);
                app.canvas().rebuild_composite();
                Log("Paste: %dx%d", app.selection().clipboard_w, app.selection().clipboard_h);
            }
        }

        if (app.selection().active) {
            // Copy
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C)) {
                fill_clipboard(app.selection(), app.canvas().active());
                Log("Copy: %dx%d region", app.selection().clipboard_w, app.selection().clipboard_h);
            }
            // Cut
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_X)) {
                if (app.canvas().active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    fill_clipboard(app.selection(), app.canvas().active());
                    int sw = app.selection().clipboard_w, sh = app.selection().clipboard_h;
                    app.canvas().push_snapshot();
                    Canvas& c = app.canvas().active();
                    for (int y = 0; y < sh; y++)
                        for (int x = 0; x < sw; x++)
                            if (app.selection().mask_selected(app.selection().x0 + x, app.selection().y0 + y))
                                c.set(app.selection().x0 + x, app.selection().y0 + y, 0x00000000);
                    app.canvas().rebuild_composite();
                    Log("Cut: %dx%d region", sw, sh);
                }
            }
            // Delete
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                if (app.canvas().active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    int sw = app.selection().width(), sh = app.selection().height();
                    app.canvas().push_snapshot();
                    Canvas& c = app.canvas().active();
                    for (int y = 0; y < sh; y++)
                        for (int x = 0; x < sw; x++)
                            if (app.selection().mask_selected(app.selection().x0 + x, app.selection().y0 + y))
                                c.set(app.selection().x0 + x, app.selection().y0 + y, 0x00000000);
                    app.canvas().rebuild_composite();
                    Log("Delete selection: %dx%d region", sw, sh);
                }
            }
            // Deselect / cancel floating selection
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (app.selection().floating) {
                    app.canvas().undo();
                    app.selection().floating = false;
                    app.selection().float_pixels.clear();
                }
                app.selection().active = false;
                app.selection().mask.clear();
            }
        }
    }
    } // !any_popup
}

} // namespace input_handler
