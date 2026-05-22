cmake_minimum_required(VERSION 3.20)
project(pix-cells VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.14
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

# lunasvg — SVG rasterizer
FetchContent_Declare(
    lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG        v3.2.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lunasvg)

# pix-cells-core — core logic as a static library (linkable by tests)
add_library(pix-cells-core STATIC
    src/app_state.cpp
    src/canvas_state.cpp
    src/log.cpp
    src/png_io.cpp
    src/pixc_io.cpp
    src/raster.cpp
)
target_include_directories(pix-cells-core PUBLIC
    src
    ${stb_SOURCE_DIR}
)
target_compile_options(pix-cells-core PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra -Wno-unused-parameter>)
target_link_libraries(pix-cells-core PUBLIC imgui_lib)

add_executable(pix-cells
    src/main.cpp
    src/icon_manager.cpp
    src/cursor_manager.cpp
    src/workbench.cpp
    src/panels/menu_bar.cpp
    src/panels/canvas_panel.cpp
    src/panels/tools_panel.cpp
    src/panels/palette_panel.cpp
    src/panels/layers_panel.cpp
    src/panels/timeline_panel.cpp
    src/panels/log_panel.cpp
)
target_compile_options(pix-cells PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra -Wno-unused-parameter>)
target_link_libraries(pix-cells PRIVATE pix-cells-core SDL3::SDL3-static OpenGL::GL lunasvg)

add_custom_command(TARGET pix-cells POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:pix-cells>/fonts
)
add_custom_command(TARGET pix-cells POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/icons $<TARGET_FILE_DIR:pix-cells>/icons
)

enable_testing()

foreach(t test_pixc test_blend test_composite test_frames test_history test_raster)
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE pix-cells-core)
    target_compile_options(${t} PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra -Wno-unused-parameter>)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
