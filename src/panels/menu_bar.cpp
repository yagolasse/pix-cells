#include "menu_bar.h"
#include "imgui.h"
#include "png_io.h"
#include "pixc_io.h"
#include "ui_scale.h"
#include <algorithm>
#include <cstring>

// --- SDL3 async file dialog helpers ---

enum class IOKind : uint8_t { PixcSave, PixcOpen, PngExport, SpriteSheet };

struct PendingIO {
    bool        active    = false;
    IOKind      kind      = IOKind::PixcSave;
    std::string path;
    int         doc_idx   = 0;  // which document this I/O targets
    // sprite sheet options
    png_io::SheetLayout sheet_layout = png_io::SheetLayout::Horizontal;
    int                 sheet_cols   = 4;
};

static PendingIO s_pending;

static void file_cb(void* ud, const char* const* list, int /*filter*/) {
    if (list && list[0]) {
        auto* p   = static_cast<PendingIO*>(ud);
        p->path   = list[0];
        p->active = true;
    }
}

// --- Menu bar ---

bool panels::DrawMenuBar(AppState& state, SDL_Window* window, bool& show_log, bool& quit_requested) {
    static bool             open_new_canvas           = false;
    static bool             open_canvas_settings      = false;
    static bool             open_sprite_sheet         = false;
    static bool             open_unsaved_quit         = false;
    static bool             open_preferences          = false;
    static bool             s_pending_quit_after_save = false;
    static std::vector<int> s_quit_unsaved_queue;
    static bool             s_quit_queue_continue     = false;

    bool keep_running = true;

    // --- Window title ---
    {
        std::string fname;
        if (!state.project_path().empty()) {
            auto pos = state.project_path().find_last_of("/\\");
            fname = (pos == std::string::npos) ? state.project_path()
                                               : state.project_path().substr(pos + 1);
        } else {
            fname = "Untitled";
        }
        std::string title = (state.canvas().unsaved_changes ? "*" : "") + fname + " \xe2\x80\x94 pix-cells";
        SDL_SetWindowTitle(window, title.c_str());
    }

    // --- Process any pending file I/O from a previous dialog callback ---
    if (s_pending.active) {
        switch (s_pending.kind) {
        case IOKind::PixcSave:
            if (s_pending.doc_idx >= 0 && s_pending.doc_idx < (int)state.docs.size()) {
                int prev = state.active_doc;
                state.active_doc = s_pending.doc_idx;
                if (pixc_io::save(state, s_pending.path)) {
                    state.canvas().unsaved_changes = false;
                    state.project_path() = s_pending.path;
                }
                state.active_doc = prev;
            }
            if (s_pending_quit_after_save) keep_running = false;
            s_pending_quit_after_save = false;
            if (s_quit_queue_continue) {
                s_quit_queue_continue = false;
                int closed_doc = s_pending.doc_idx;
                if (closed_doc >= 0 && closed_doc < (int)state.docs.size())
                    state.close_doc_idx_requested = closed_doc;
                if (!s_quit_unsaved_queue.empty())
                    s_quit_unsaved_queue.erase(s_quit_unsaved_queue.begin());
                for (auto& idx : s_quit_unsaved_queue)
                    if (idx > closed_doc) --idx;
                if (s_quit_unsaved_queue.empty()) keep_running = false;
                else open_unsaved_quit = true;
            }
            break;

        case IOKind::PixcOpen: {
            FILE* mf = fopen(s_pending.path.c_str(), "rb");
            char magic[4] = {};
            if (mf) { fread(magic, 1, 4, mf); fclose(mf); }
            if (memcmp(magic, "PIXC", 4) == 0) {
                state.docs.emplace_back();
                state.active_doc = (int)state.docs.size() - 1;
                if (pixc_io::load(state, s_pending.path)) {
                    state.project_path() = s_pending.path;
                    state.canvas().unsaved_changes = false;
                } else {
                    state.docs.pop_back();
                    state.active_doc = std::min(state.active_doc, (int)state.docs.size() - 1);
                }
            } else {
                Canvas tmp;
                if (png_io::load(tmp, s_pending.path)) {
                    state.docs.emplace_back();
                    state.active_doc = (int)state.docs.size() - 1;
                    state.canvas().new_canvas(tmp.width, tmp.height);
                    state.canvas().frames[0].layers[0].canvas = std::move(tmp);
                    state.canvas().rebuild_composite();
                    state.project_path() = s_pending.path;
                    state.canvas().unsaved_changes = false;
                }
            }
            break;
        }

        case IOKind::PngExport: {
            int prev = state.active_doc;
            if (s_pending.doc_idx < (int)state.docs.size())
                state.active_doc = s_pending.doc_idx;
            Canvas tmp(state.canvas().width(), state.canvas().height());
            tmp.pixels = state.canvas().composite;
            png_io::save(tmp, s_pending.path);
            state.active_doc = prev;
            break;
        }

        case IOKind::SpriteSheet: {
            int prev = state.active_doc;
            if (s_pending.doc_idx < (int)state.docs.size())
                state.active_doc = s_pending.doc_idx;
            png_io::save_sprite_sheet(state.canvas(), s_pending.path,
                                      s_pending.sheet_layout, s_pending.sheet_cols);
            state.active_doc = prev;
            break;
        }
        }
        s_pending.active = false;
    }

    // --- Quick save: to current path if known, else open dialog ---
    static SDL_DialogFileFilter s_pixc_filter[] = { {"PIXC Files", "pixc"} };

    auto do_save = [&](bool quit_after) {
        const auto& p = state.project_path();
        bool is_pixc = p.size() >= 5 &&
                       (p.compare(p.size() - 5, 5, ".pixc") == 0 ||
                        p.compare(p.size() - 5, 5, ".PIXC") == 0);
        if (!p.empty() && is_pixc) {
            if (pixc_io::save(state, p)) {
                state.canvas().unsaved_changes = false;
                if (quit_after) keep_running = false;
            }
        } else {
            s_pending.kind = IOKind::PixcSave;
            s_pending.doc_idx = state.active_doc;
            s_pending_quit_after_save = quit_after;
            SDL_ShowSaveFileDialog(file_cb, &s_pending, window, s_pixc_filter, 1, nullptr);
        }
    };

    // --- Keyboard shortcuts ---
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        do_save(false);
    // Ctrl+N always opens a new tab (no unsaved-changes warning)
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
        open_new_canvas = true;

    // --- Collect all dirty docs and start the per-doc quit confirmation queue ---
    auto trigger_quit = [&]() {
        s_quit_unsaved_queue.clear();
        for (int i = 0; i < (int)state.docs.size(); i++) {
            if (state.docs[i].canvas.unsaved_changes)
                s_quit_unsaved_queue.push_back(i);
        }
        if (s_quit_unsaved_queue.empty()) keep_running = false;
        else open_unsaved_quit = true;
    };

    // --- Handle window-close quit request ---
    if (quit_requested) {
        quit_requested = false;
        trigger_quit();
    }

    // --- Menu bar ---
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // New always opens a new tab
            if (ImGui::MenuItem("New", "Ctrl+N"))
                open_new_canvas = true;

            static SDL_DialogFileFilter open_filters[] = { {"PIXC Files", "pixc"}, {"PNG Images", "png"} };
            static SDL_DialogFileFilter png_filter[]   = { {"PNG Images", "png"} };

            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                s_pending.kind = IOKind::PixcOpen;
                s_pending.doc_idx = state.active_doc;
                SDL_ShowOpenFileDialog(file_cb, &s_pending, window,
                                       open_filters, 2, nullptr, false);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S"))
                do_save(false);
            if (ImGui::MenuItem("Save As...")) {
                s_pending.kind = IOKind::PixcSave;
                s_pending.doc_idx = state.active_doc;
                s_pending_quit_after_save = false;
                SDL_ShowSaveFileDialog(file_cb, &s_pending, window, s_pixc_filter, 1, nullptr);
            }
            if (ImGui::MenuItem("Close"))
                state.close_active_doc_requested = true;

            if (ImGui::BeginMenu("Export")) {
                if (ImGui::MenuItem("PNG (current frame)...")) {
                    s_pending.kind = IOKind::PngExport;
                    s_pending.doc_idx = state.active_doc;
                    SDL_ShowSaveFileDialog(file_cb, &s_pending, window,
                                           png_filter, 1, nullptr);
                }
                if (ImGui::MenuItem("Sprite Sheet...")) {
                    open_sprite_sheet = true;
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4"))
                trigger_quit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                state.canvas().undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                state.canvas().redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Canvas Settings..."))
                open_canvas_settings = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences..."))
                open_preferences = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Log", nullptr, &show_log);
            ImGui::MenuItem("Preview", nullptr, &state.tools.show_preview);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- Unsaved Changes modal (quit) — one dialog per dirty doc ---
    if (open_unsaved_quit) { ImGui::OpenPopup("Unsaved Changes##quit"); open_unsaved_quit = false; }
    if (ImGui::BeginPopupModal("Unsaved Changes##quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int doc_idx = s_quit_unsaved_queue.empty() ? -1 : s_quit_unsaved_queue.front();
        if (doc_idx >= 0 && doc_idx < (int)state.docs.size()) {
            const auto& dp = state.docs[doc_idx].project_path;
            std::string fname;
            if (!dp.empty()) {
                auto pos = dp.find_last_of("/\\");
                fname = (pos == std::string::npos) ? dp : dp.substr(pos + 1);
            } else {
                fname = "Untitled";
            }
            ImGui::Text("Save \"%s\" before quitting?", fname.c_str());
        }
        ImGui::Spacing();

        // Close the confirmed tab and advance to the next dirty doc.
        auto advance_queue = [&](int closed_idx) {
            if (closed_idx >= 0 && closed_idx < (int)state.docs.size()) {
                state.docs[closed_idx].canvas.unsaved_changes = false;
                state.close_doc_idx_requested = closed_idx;
            }
            s_quit_unsaved_queue.erase(s_quit_unsaved_queue.begin());
            for (auto& idx : s_quit_unsaved_queue)
                if (idx > closed_idx) --idx;
            if (s_quit_unsaved_queue.empty()) keep_running = false;
            else open_unsaved_quit = true;
        };

        if (ImGui::Button("Save")) {
            if (doc_idx >= 0 && doc_idx < (int)state.docs.size()) {
                const auto& p = state.docs[doc_idx].project_path;
                bool is_pixc  = p.size() >= 5 &&
                                (p.compare(p.size() - 5, 5, ".pixc") == 0 ||
                                 p.compare(p.size() - 5, 5, ".PIXC") == 0);
                if (!p.empty() && is_pixc) {
                    int prev = state.active_doc;
                    state.active_doc = doc_idx;
                    pixc_io::save(state, p);
                    state.active_doc = prev;
                    advance_queue(doc_idx);
                } else {
                    s_pending.kind            = IOKind::PixcSave;
                    s_pending.doc_idx         = doc_idx;
                    s_pending_quit_after_save = false;
                    s_quit_queue_continue     = true;
                    SDL_ShowSaveFileDialog(file_cb, &s_pending, window, s_pixc_filter, 1, nullptr);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save")) {
            advance_queue(doc_idx);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            s_quit_unsaved_queue.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Canvas Settings popup ---
    if (open_canvas_settings) {
        ImGui::OpenPopup("Canvas Settings");
        open_canvas_settings = false;
    }
    if (ImGui::BeginPopupModal("Canvas Settings", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SliderFloat("Cell Size", &state.canvas().checker_size, 2.f, 64.f, "%.0f px");
        ImGui::ColorEdit3("Light Color", (float*)&state.canvas().checker_color1);
        ImGui::ColorEdit3("Dark Color",  (float*)&state.canvas().checker_color2);
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- Preferences popup ---
    if (open_preferences) { ImGui::OpenPopup("Preferences"); open_preferences = false; }
    if (ImGui::BeginPopupModal("Preferences", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* scale_labels[] = {"75%", "100%", "125%", "150%", "200%"};
        static const float scale_values[] = {0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
        float cur = ui_scale::get();
        int sel = 1;
        for (int i = 0; i < 5; ++i)
            if (cur == scale_values[i]) { sel = i; break; }
        ImGui::Text("UI Scale");
        ImGui::SetNextItemWidth(ui_scale::px(120.0f));
        if (ImGui::Combo("##uiscale", &sel, scale_labels, 5)) {
            ui_scale::apply(scale_values[sel]);
            ui_scale::save();
        }
        ImGui::Spacing();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- New Canvas popup (always creates a new tab) ---
    if (open_new_canvas) {
        ImGui::OpenPopup("New Canvas");
        open_new_canvas = false;
    }
    if (ImGui::BeginPopupModal("New Canvas", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        static int w = 64, h = 64;
        ImGui::InputInt("Width",  &w, 1, 16);
        ImGui::InputInt("Height", &h, 1, 16);
        w = std::clamp(w, 1, 4096);
        h = std::clamp(h, 1, 4096);
        if (ImGui::Button("Create")) {
            state.docs.emplace_back();
            state.active_doc = (int)state.docs.size() - 1;
            state.canvas().new_canvas(w, h);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- Sprite Sheet export popup ---
    if (open_sprite_sheet) { ImGui::OpenPopup("Sprite Sheet"); open_sprite_sheet = false; }
    if (ImGui::BeginPopupModal("Sprite Sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static int layout_idx = 0;
        ImGui::RadioButton("Horizontal", &layout_idx, 0); ImGui::SameLine();
        ImGui::RadioButton("Vertical",   &layout_idx, 1); ImGui::SameLine();
        ImGui::RadioButton("Grid",       &layout_idx, 2);
        static int cols = 4;
        if (layout_idx == 2) {
            ImGui::SetNextItemWidth(ui_scale::px(80.0f));
            ImGui::InputInt("Columns", &cols, 1, 4);
            cols = std::clamp(cols, 1, 16);
        }
        if (ImGui::Button("Export...")) {
            static SDL_DialogFileFilter png_filter_ss[] = { {"PNG Images", "png"} };
            s_pending.kind         = IOKind::SpriteSheet;
            s_pending.doc_idx      = state.active_doc;
            s_pending.sheet_layout = (png_io::SheetLayout)layout_idx;
            s_pending.sheet_cols   = cols;
            SDL_ShowSaveFileDialog(file_cb, &s_pending, window, png_filter_ss, 1, nullptr);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    return keep_running;
}
