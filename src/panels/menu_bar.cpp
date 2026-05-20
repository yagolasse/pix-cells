#include "menu_bar.h"
#include "imgui.h"
#include "png_io.h"
#include "log.h"
#include <algorithm>

// --- SDL3 async file dialog helpers ---

struct PendingIO {
    bool        active  = false;
    bool        is_save = false;
    std::string path;
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

bool panels::DrawMenuBar(AppState& state, SDL_Window* window, bool& show_log) {
    // Process any pending file I/O from a previous dialog callback
    if (s_pending.active) {
        if (s_pending.is_save) {
            Log("Saving: %s", s_pending.path.c_str());
            Canvas tmp(state.canvas.width(), state.canvas.height());
            tmp.pixels = state.canvas.composite;
            png_io::save(tmp, s_pending.path);
        } else {
            Log("Loading: %s", s_pending.path.c_str());
            Canvas tmp;
            if (png_io::load(tmp, s_pending.path)) {
                state.canvas.new_canvas(tmp.width, tmp.height);
                state.canvas.layers[0].canvas = std::move(tmp);
                state.canvas.rebuild_composite();
            }
        }
        s_pending.active = false;
    }

    static bool open_new_canvas      = false;
    static bool open_canvas_settings = false;
    bool keep_running = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New",  "Ctrl+N")) open_new_canvas = true;

            static SDL_DialogFileFilter png_filter[] = { {"PNG Images", "png"} };
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                s_pending.is_save = false;
                SDL_ShowOpenFileDialog(file_cb, &s_pending, window,
                                      png_filter, 1, nullptr, false);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                s_pending.is_save = true;
                SDL_ShowSaveFileDialog(file_cb, &s_pending, window,
                                      png_filter, 1, nullptr);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) keep_running = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) state.canvas.undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) state.canvas.redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Canvas Settings...")) open_canvas_settings = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Log", nullptr, &show_log);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Canvas Settings popup
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

    // New Canvas popup (must be outside BeginMainMenuBar scope)
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
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    return keep_running;
}
