#include "menu_bar.h"
#include "imgui.h"
#include "png_io.h"
#include "pixc_io.h"
#include "log.h"
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

bool panels::DrawMenuBar(AppState& state, SDL_Window* window) {
    // Process any pending file I/O from a previous dialog callback
    if (s_pending.active) {
        switch (s_pending.kind) {
        case IOKind::PixcSave:
            pixc_io::save(state, s_pending.path);
            break;

        case IOKind::PixcOpen: {
            // Detect format by magic bytes
            FILE* mf = fopen(s_pending.path.c_str(), "rb");
            char magic[4] = {};
            if (mf) { fread(magic, 1, 4, mf); fclose(mf); }
            if (memcmp(magic, "PIXC", 4) == 0) {
                pixc_io::load(state, s_pending.path);
            } else {
                Canvas tmp;
                if (png_io::load(tmp, s_pending.path)) {
                    state.canvas.new_canvas(tmp.width, tmp.height);
                    state.canvas.frames[0].layers[0].canvas = std::move(tmp);
                    state.canvas.rebuild_composite();
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

    static bool open_new_canvas      = false;
    static bool open_canvas_settings = false;
    static bool open_sprite_sheet    = false;
    bool keep_running = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New",  "Ctrl+N")) open_new_canvas = true;

            static SDL_DialogFileFilter pixc_filter[]  = { {"PIXC Files", "pixc"} };
            static SDL_DialogFileFilter open_filters[] = { {"PIXC Files", "pixc"}, {"PNG Images", "png"} };
            static SDL_DialogFileFilter png_filter[]   = { {"PNG Images", "png"} };

            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                s_pending.kind = IOKind::PixcOpen;
                SDL_ShowOpenFileDialog(file_cb, &s_pending, window,
                                       open_filters, 2, nullptr, false);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                s_pending.kind = IOKind::PixcSave;
                SDL_ShowSaveFileDialog(file_cb, &s_pending, window,
                                       pixc_filter, 1, nullptr);
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
            if (ImGui::MenuItem("Quit", "Alt+F4")) keep_running = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) state.canvas.undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) state.canvas.redo();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Canvas Settings...")) open_canvas_settings = true;
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

    // New Canvas popup
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

    // Sprite Sheet export popup
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
