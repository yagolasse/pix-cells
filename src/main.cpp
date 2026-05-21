#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "IconsFontAwesome6.h"
#include "workbench.h"
#include "app_state.h"
#include "panels/menu_bar.h"
#include "panels/canvas_panel.h"
#include "panels/tools_panel.h"
#include "panels/palette_panel.h"
#include "panels/layers_panel.h"
#include "panels/timeline_panel.h"
#include "panels/log_panel.h"
#include "log.h"
#include <algorithm>

int main(int /*argc*/, char* /*argv*/[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("pix-cells", 1280, 720,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    // Mocha warm-dark theme
    {
        ImVec4* c = ImGui::GetStyle().Colors;
        // Base surfaces
        c[ImGuiCol_WindowBg]         = ImVec4(0.133f, 0.122f, 0.106f, 1.00f); // #221f1b
        c[ImGuiCol_ChildBg]          = ImVec4(0.082f, 0.075f, 0.059f, 1.00f); // #15130f
        c[ImGuiCol_PopupBg]          = ImVec4(0.165f, 0.149f, 0.125f, 1.00f); // #2a2620
        c[ImGuiCol_TitleBg]          = ImVec4(0.165f, 0.149f, 0.125f, 1.00f); // #2a2620
        c[ImGuiCol_TitleBgActive]    = ImVec4(0.208f, 0.184f, 0.145f, 1.00f); // #352f25
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.165f, 0.149f, 0.125f, 1.00f);
        c[ImGuiCol_MenuBarBg]        = ImVec4(0.149f, 0.133f, 0.110f, 1.00f); // #26221c
        // Text
        c[ImGuiCol_Text]         = ImVec4(0.941f, 0.910f, 0.839f, 1.00f); // #f0e8d6
        c[ImGuiCol_TextDisabled] = ImVec4(0.353f, 0.318f, 0.271f, 1.00f); // #5a5145
        // Frames / inputs
        c[ImGuiCol_FrameBg]        = ImVec4(0.094f, 0.086f, 0.071f, 1.00f); // #181612
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.122f, 0.110f, 0.090f, 1.00f); // #1f1c17
        c[ImGuiCol_FrameBgActive]  = ImVec4(0.122f, 0.110f, 0.090f, 1.00f);
        // Borders
        c[ImGuiCol_Border]           = ImVec4(0.549f, 0.486f, 0.392f, 0.28f);
        c[ImGuiCol_BorderShadow]     = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
        c[ImGuiCol_Separator]        = ImVec4(0.549f, 0.486f, 0.392f, 0.20f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(0.549f, 0.486f, 0.392f, 0.40f);
        c[ImGuiCol_SeparatorActive]  = ImVec4(0.549f, 0.486f, 0.392f, 0.60f);
        // Scrollbar
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.000f, 0.000f, 0.000f, 0.35f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.290f, 0.267f, 0.227f, 1.00f); // #4a443a
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.353f, 0.325f, 0.275f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.420f, 0.388f, 0.330f, 1.00f);
        // Accent: #d4a04a = (0.831, 0.627, 0.290)
        c[ImGuiCol_Header]           = ImVec4(0.831f, 0.627f, 0.290f, 0.30f);
        c[ImGuiCol_HeaderHovered]    = ImVec4(0.831f, 0.627f, 0.290f, 0.55f);
        c[ImGuiCol_HeaderActive]     = ImVec4(0.831f, 0.627f, 0.290f, 0.92f);
        c[ImGuiCol_Button]           = ImVec4(0.831f, 0.627f, 0.290f, 0.15f);
        c[ImGuiCol_ButtonHovered]    = ImVec4(0.831f, 0.627f, 0.290f, 0.35f);
        c[ImGuiCol_ButtonActive]     = ImVec4(0.831f, 0.627f, 0.290f, 0.65f);
        c[ImGuiCol_CheckMark]        = ImVec4(0.831f, 0.627f, 0.290f, 1.00f);
        c[ImGuiCol_SliderGrab]       = ImVec4(0.831f, 0.627f, 0.290f, 1.00f);
        c[ImGuiCol_SliderGrabActive] = ImVec4(0.900f, 0.720f, 0.420f, 1.00f);
        c[ImGuiCol_DockingPreview]   = ImVec4(0.831f, 0.627f, 0.290f, 0.40f);
        c[ImGuiCol_DragDropTarget]   = ImVec4(0.831f, 0.627f, 0.290f, 0.90f);
        // Tabs
        c[ImGuiCol_Tab]                 = ImVec4(0.165f, 0.149f, 0.125f, 1.00f);
        c[ImGuiCol_TabHovered]          = ImVec4(0.208f, 0.184f, 0.145f, 1.00f);
        c[ImGuiCol_TabSelected]         = ImVec4(0.831f, 0.627f, 0.290f, 0.30f);
        c[ImGuiCol_TabSelectedOverline] = ImVec4(0.831f, 0.627f, 0.290f, 1.00f);
        c[ImGuiCol_TabDimmed]           = ImVec4(0.133f, 0.122f, 0.106f, 1.00f);
        c[ImGuiCol_TabDimmedSelected]   = ImVec4(0.165f, 0.149f, 0.125f, 1.00f);
        // Resize grip
        c[ImGuiCol_ResizeGrip]        = ImVec4(0.831f, 0.627f, 0.290f, 0.20f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.831f, 0.627f, 0.290f, 0.50f);
        c[ImGuiCol_ResizeGripActive]  = ImVec4(0.831f, 0.627f, 0.290f, 0.80f);
        // Rounding
        ImGui::GetStyle().WindowRounding    = 4.0f;
        ImGui::GetStyle().FrameRounding     = 3.0f;
        ImGui::GetStyle().GrabRounding      = 3.0f;
        ImGui::GetStyle().TabRounding       = 3.0f;
        ImGui::GetStyle().ScrollbarRounding = 3.0f;
    }

    io.Fonts->AddFontFromFileTTF("fonts/Ubuntu-Regular.ttf", 15.0f);
    {
        static const ImWchar ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        ImFont* icon_font             = io.Fonts->AddFontFromFileTTF("fonts/fa-solid-900.ttf", 16.0f, nullptr, ranges);
        panels::SetIconFont(icon_font);
        panels::SetPaletteIconFont(icon_font);
        panels::SetLayersIconFont(icon_font);
        panels::SetTimelineIconFont(icon_font);
    }

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    AppState app;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
            app.canvas.undo();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y))
            app.canvas.redo();

        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_B)) {
                app.tools.active_tool = 0;
                Log("Tool: Brush");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) {
                app.tools.active_tool = 1;
                Log("Tool: Eraser");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                app.tools.active_tool = 2;
                Log("Tool: Fill");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_L)) {
                app.tools.active_tool = 3;
                Log("Tool: Line");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                app.tools.active_tool = 4;
                Log("Tool: Rect");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_U)) {
                app.tools.active_tool = 6;
                Log("Tool: Circle");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_M)) {
                app.tools.active_tool = 8;
                Log("Tool: Move");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_S)) {
                app.tools.active_tool = 9;
                Log("Tool: Rect Select");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_I)) {
                app.tools.active_tool = 10;
                Log("Tool: Color Picker");
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
                app.selection.active = true;
                app.selection.x0 = 0; app.selection.y0 = 0;
                app.selection.x1 = app.canvas.width() - 1;
                app.selection.y1 = app.canvas.height() - 1;
                Log("Select All");
            }

            // Paste (works any time clipboard is non-empty)
            if (!app.selection.clipboard.empty() &&
                ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
                if (app.canvas.active_layer_locked()) {
                    Log("Layer locked");
                } else {
                    app.canvas.push_snapshot();
                    Canvas& c = app.canvas.active();
                    for (int y = 0; y < app.selection.clipboard_h; y++)
                        for (int x = 0; x < app.selection.clipboard_w; x++)
                            c.set(app.selection.clipboard_ox + x, app.selection.clipboard_oy + y,
                                  app.selection.clipboard[y * app.selection.clipboard_w + x]);
                    app.canvas.rebuild_composite();
                    Log("Paste: %dx%d", app.selection.clipboard_w, app.selection.clipboard_h);
                }
            }

            if (app.selection.active) {
                // Copy
                if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C)) {
                    int sw = app.selection.x1 - app.selection.x0 + 1;
                    int sh = app.selection.y1 - app.selection.y0 + 1;
                    app.selection.clipboard.resize(sw * sh);
                    app.selection.clipboard_w  = sw;
                    app.selection.clipboard_h  = sh;
                    app.selection.clipboard_ox = app.selection.x0;
                    app.selection.clipboard_oy = app.selection.y0;
                    const Canvas& c = app.canvas.active();
                    for (int y = 0; y < sh; y++)
                        for (int x = 0; x < sw; x++)
                            app.selection.clipboard[y * sw + x] = c.get(app.selection.x0 + x, app.selection.y0 + y);
                    Log("Copy: %dx%d region", sw, sh);
                }
                // Cut
                if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_X)) {
                    if (app.canvas.active_layer_locked()) {
                        Log("Layer locked");
                    } else {
                        int sw = app.selection.x1 - app.selection.x0 + 1;
                        int sh = app.selection.y1 - app.selection.y0 + 1;
                        app.selection.clipboard.resize(sw * sh);
                        app.selection.clipboard_w  = sw;
                        app.selection.clipboard_h  = sh;
                        app.selection.clipboard_ox = app.selection.x0;
                        app.selection.clipboard_oy = app.selection.y0;
                        Canvas& c = app.canvas.active();
                        for (int y = 0; y < sh; y++)
                            for (int x = 0; x < sw; x++)
                                app.selection.clipboard[y * sw + x] = c.get(app.selection.x0 + x, app.selection.y0 + y);
                        app.canvas.push_snapshot();
                        for (int y = 0; y < sh; y++)
                            for (int x = 0; x < sw; x++)
                                c.set(app.selection.x0 + x, app.selection.y0 + y, 0x00000000);
                        app.canvas.rebuild_composite();
                        Log("Cut: %dx%d region", sw, sh);
                    }
                }
                // Delete
                if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                    if (app.canvas.active_layer_locked()) {
                        Log("Layer locked");
                    } else {
                        int sw = app.selection.x1 - app.selection.x0 + 1;
                        int sh = app.selection.y1 - app.selection.y0 + 1;
                        app.canvas.push_snapshot();
                        Canvas& c = app.canvas.active();
                        for (int y = 0; y < sh; y++)
                            for (int x = 0; x < sw; x++)
                                c.set(app.selection.x0 + x, app.selection.y0 + y, 0x00000000);
                        app.canvas.rebuild_composite();
                        Log("Delete selection: %dx%d region", sw, sh);
                    }
                }
                // Deselect / cancel floating selection
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    if (app.selection.floating) {
                        app.canvas.undo();
                        app.selection.floating = false;
                        app.selection.float_pixels.clear();
                    }
                    app.selection.active = false;
                }
            }
        }

        BeginWorkbench();

        static bool show_log = false;
        if (!panels::DrawMenuBar(app, window, show_log))
            running = false;

        panels::DrawTools(app.tools);
        panels::DrawCanvas(app.canvas, app.tools, app.palette, app.selection);
        panels::DrawLayers(app.canvas);
        panels::DrawTimeline(app.canvas);
        panels::DrawPalette(app.palette);
        if (show_log)
            panels::DrawLog(&show_log);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.102f, 0.094f, 0.082f, 1.0f); // #1a1815
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
