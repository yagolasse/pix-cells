cmake_minimum_required(VERSION 3.20)
project(pix-cells VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# SDL3 — pin to a release tag for production (e.g. release-3.2.14)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# Dear ImGui (docking branch) — no CMakeLists.txt, so we populate manually
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL3::SDL3-static)

find_package(OpenGL REQUIRED)

# stb — header-only image load/write
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

add_executable(pix-cells
    src/main.cpp
    src/workbench.cpp
    src/app_state.cpp
    src/canvas_state.cpp
    src/log.cpp
    src/png_io.cpp
    src/panels/menu_bar.cpp
    src/panels/canvas_panel.cpp
    src/panels/tools_panel.cpp
    src/panels/palette_panel.cpp
    src/panels/layers_panel.cpp
    src/panels/log_panel.cpp
)
target_include_directories(pix-cells PRIVATE src ${stb_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/vendor/icons_font_awesome)

add_custom_command(TARGET pix-cells POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:pix-cells>/fonts
)
target_link_libraries(pix-cells PRIVATE imgui_lib SDL3::SDL3-static OpenGL::GL)
