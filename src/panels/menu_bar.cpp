#include "menu_bar.h"
#include "imgui.h"
#include "png_io.h"
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
        auto* p  = static_cast<PendingIO*>(ud);
        p->path  = list[0];
        p->active = true;
    }
}

// --- Menu bar ---

bool panels::DrawMenuBar(AppState& state, SDL_Window* window) {
    // Process any pending file I/O from a previous dialog callback
    if (s_pending.active) {
        if (s_pending.is_save)
            png_io::save(state.canvas.canvas, s_pending.path);
        else {
            png_io::load(state.canvas.canvas, s_pending.path);
            state.canvas.dirty = true;
        }
        s_pending.active = false;
    }

    static bool open_new_canvas = false;
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
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
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
            state.canvas.canvas.resize(w, h);
            state.canvas.pan   = { 0.0f, 0.0f };
            state.canvas.dirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    return keep_running;
}
