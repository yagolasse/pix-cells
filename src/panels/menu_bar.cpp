#include "menu_bar.h"
#include "imgui.h"
#include "png_io.h"
#include "pixc_io.h"
#include <algorithm>
#include <cstring>

// --- SDL3 async file dialog helpers ---

enum class IOKind { PixcSave, PixcOpen, PngExport, SpriteSheet };

struct PendingIO {
    bool        active = false;
    IOKind      kind   = IOKind::PixcSave;
    std::string path;
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
    static bool open_new_canvas           = false;
    static bool open_canvas_settings      = false;
    static bool open_sprite_sheet         = false;
    static bool open_unsaved_quit         = false;
    static bool open_unsaved_new          = false;
    static bool s_pending_quit_after_save = false;
    static bool s_pending_new_after_save  = false;

    bool keep_running = true;

    // --- Window title ---
    {
        std::string fname;
        if (!state.project_path.empty()) {
            auto pos = state.project_path.find_last_of("/\\");
            fname = (pos == std::string::npos) ? state.project_path
                                               : state.project_path.substr(pos + 1);
        } else {
            fname = "Untitled";
        }
        std::string title = (state.canvas.unsaved_changes ? "*" : "") + fname + " \xe2\x80\x94 pix-cells";
        SDL_SetWindowTitle(window, title.c_str());
    }

    // --- Process any pending file I/O from a previous dialog callback ---
    if (s_pending.active) {
        switch (s_pending.kind) {
        case IOKind::PixcSave:
            if (pixc_io::save(state, s_pending.path)) {
                state.canvas.unsaved_changes = false;
                state.project_path = s_pending.path;
            }
            if (s_pending_quit_after_save) keep_running = false;
            if (s_pending_new_after_save)  open_new_canvas = true;
            s_pending_quit_after_save = false;
            s_pending_new_after_save  = false;
            break;

        case IOKind::PixcOpen: {
            FILE* mf = fopen(s_pending.path.c_str(), "rb");
            char magic[4] = {};
            if (mf) { fread(magic, 1, 4, mf); fclose(mf); }
            if (memcmp(magic, "PIXC", 4) == 0) {
                if (pixc_io::load(state, s_pending.path)) {
                    state.project_path = s_pending.path;
                    state.canvas.unsaved_changes = false;
                }
            } else {
                Canvas tmp;
                if (png_io::load(tmp, s_pending.path)) {
                    state.canvas.new_canvas(tmp.width, tmp.height);
                    state.canvas.frames[0].layers[0].canvas = std::move(tmp);
                    state.canvas.rebuild_composite();
                    state.project_path.clear();
                    state.canvas.unsaved_changes = false;
                }
            }
            break;
        }

        case IOKind::PngExport: {
            Canvas tmp(state.canvas.width(), state.canvas.height());
            tmp.pixels = state.canvas.composite;
            png_io::save(tmp, s_pending.path);
            break;
        }

        case IOKind::SpriteSheet:
            png_io::save_sprite_sheet(state.canvas, s_pending.path,
                                      s_pending.sheet_layout, s_pending.sheet_cols);
            break;
        }
        s_pending.active = false;
    }

    // --- Quick save: to current path if known, else open dialog ---
    static SDL_DialogFileFilter s_pixc_filter[] = { {"PIXC Files", "pixc"} };

    auto do_save = [&](bool quit_after, bool new_after) {
        if (!state.project_path.empty()) {
            if (pixc_io::save(state, state.project_path)) {
                state.canvas.unsaved_changes = false;
                if (quit_after) keep_running = false;
                if (new_after)  open_new_canvas = true;
            }
        } else {
            s_pending.kind = IOKind::PixcSave;
            s_pending_quit_after_save = quit_after;
            s_pending_new_after_save  = new_after;
            SDL_ShowSaveFileDialog(file_cb, &s_pending, window, s_pixc_filter, 1, nullptr);
        }
    };

    // --- Keyboard shortcuts ---
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        do_save(false, false);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
        if (state.canvas.unsaved_changes) open_unsaved_new = true;
        else open_new_canvas = true;
    }

    // --- Handle window-close quit request ---
    if (quit_requested) {
        quit_requested = false;
        if (state.canvas.unsaved_changes) open_unsaved_quit = true;
        else keep_running = false;
    }

    // --- Menu bar ---
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                if (state.canvas.unsaved_changes) open_unsaved_new = true;
                else open_new_canvas = true;
            }

            static SDL_DialogFileFilter open_filters[] = { {"PIXC Files", "pixc"}, {"PNG Images", "png"} };
            static SDL_DialogFileFilter png_filter[]   = { {"PNG Images", "png"} };

            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                s_pending.kind = IOKind::PixcOpen;
                SDL_ShowOpenFileDialog(file_cb, &s_pending, window,
                                       open_filters, 2, nullptr, false);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S"))
                do_save(false, false);
            if (ImGui::MenuItem("Save As...")) {
                s_pending.kind = IOKind::PixcSave;
                s_pending_quit_after_save = false;
                s_pending_new_after_save  = false;
                SDL_ShowSaveFileDialog(file_cb, &s_pending, window, s_pixc_filter, 1, nullptr);
            }

            if (ImGui::BeginMenu("Export")) {
                if (ImGui::MenuItem("PNG (current frame)...")) {
                    s_pending.kind = IOKind::PngExport;
                    SDL_ShowSaveFileDialog(file_cb, &s_pending, window,
                                           png_filter, 1, nullptr);
                }
                if (ImGui::MenuItem("Sprite Sheet...")) {
                    open_sprite_sheet = true;
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                if (state.canvas.unsaved_changes) open_unsaved_quit = true;
                else keep_running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                state.canvas.undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                state.canvas.redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Canvas Settings..."))
                open_canvas_settings = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Log", nullptr, &show_log);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- Unsaved Changes modal (quit) ---
    if (open_unsaved_quit) { ImGui::OpenPopup("Unsaved Changes##quit"); open_unsaved_quit = false; }
    if (ImGui::BeginPopupModal("Unsaved Changes##quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Save before quitting?");
        ImGui::Spacing();
        if (ImGui::Button("Save")) {
            do_save(true, false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save")) {
            keep_running = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- Unsaved Changes modal (new canvas) ---
    if (open_unsaved_new) { ImGui::OpenPopup("Unsaved Changes##new"); open_unsaved_new = false; }
    if (ImGui::BeginPopupModal("Unsaved Changes##new", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Save before creating a new canvas?");
        ImGui::Spacing();
        if (ImGui::Button("Save")) {
            do_save(false, true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save")) {
            open_new_canvas = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- Canvas Settings popup ---
    if (open_canvas_settings) {
        ImGui::OpenPopup("Canvas Settings");
        open_canvas_settings = false;
    }
    if (ImGui::BeginPopupModal("Canvas Settings", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SliderFloat("Cell Size", &state.canvas.checker_size, 2.f, 64.f, "%.0f px");
        ImGui::ColorEdit3("Light Color", (float*)&state.canvas.checker_color1);
        ImGui::ColorEdit3("Dark Color",  (float*)&state.canvas.checker_color2);
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- New Canvas popup ---
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
            state.canvas.new_canvas(w, h);
            state.project_path.clear();
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
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Columns", &cols, 1, 4);
            cols = std::clamp(cols, 1, 16);
        }
        if (ImGui::Button("Export...")) {
            static SDL_DialogFileFilter png_filter_ss[] = { {"PNG Images", "png"} };
            s_pending.kind         = IOKind::SpriteSheet;
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
