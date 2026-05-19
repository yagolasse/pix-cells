#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "workbench.h"
#include "app_state.h"
#include "panels/menu_bar.h"
#include "panels/canvas_panel.h"
#include "panels/tools_panel.h"
#include "panels/palette_panel.h"
#include "panels/layers_panel.h"
#include "panels/log_panel.h"
#include "log.h"
#include <algorithm>

int main(int /*argc*/, char* /*argv*/[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "pix-cells",
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

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

        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) app.canvas.undo();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) app.canvas.redo();

        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_B)) { app.tools.active_tool = 0; Log("Tool: Brush"); }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) { app.tools.active_tool = 1; Log("Tool: Eraser"); }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) { app.tools.active_tool = 2; Log("Tool: Fill"); }
            if (ImGui::IsKeyPressed(ImGuiKey_L)) { app.tools.active_tool = 3; Log("Tool: Line"); }
            if (ImGui::IsKeyPressed(ImGuiKey_R)) { app.tools.active_tool = 4; Log("Tool: Rect"); }
            if (ImGui::IsKeyPressed(ImGuiKey_C)) { app.tools.active_tool = 5; Log("Tool: Circle"); }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
                app.tools.brush_size = std::max(1, app.tools.brush_size - 1);
                Log("Brush size: %d", app.tools.brush_size);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
                app.tools.brush_size = std::min(32, app.tools.brush_size + 1);
                Log("Brush size: %d", app.tools.brush_size);
            }
        }

        ImGuiID dock_id = BeginWorkbench();
        EnsureDefaultLayout(dock_id);

        if (!panels::DrawMenuBar(app, window))
            running = false;

        panels::DrawTools(app.tools);
        panels::DrawCanvas(app.canvas, app.tools, app.palette);
        panels::DrawLayers(app.canvas);
        panels::DrawPalette(app.palette);
        panels::DrawLog();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
