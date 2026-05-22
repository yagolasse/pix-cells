#include "cursor_manager.h"
#include <SDL3/SDL.h>
#include <lunasvg.h>
#include <string>
#include <cstring>

namespace {
    constexpr int CURSOR_SIZE = 32;

    std::string s_icons_dir;

    SDL_Cursor* s_crosshair    = nullptr;
    SDL_Cursor* s_eyedropper   = nullptr;
    SDL_Cursor* s_pan          = nullptr;
    SDL_Cursor* s_pan_pressed  = nullptr;

    // hotspot_x, hotspot_y for each cursor (0-indexed within CURSOR_SIZE image)
    // point_scan: center
    constexpr int CROSS_HOT_X = 16, CROSS_HOT_Y = 16;
    // eyedropper (colorize): dropper tip at bottom-left
    constexpr int EYE_HOT_X   = 6,  EYE_HOT_Y  = 26;
    // pan_tool / pan_tool_alt: palm center
    constexpr int PAN_HOT_X   = 16, PAN_HOT_Y  = 16;

    SDL_Cursor* load_cursor(const char* name, int hotx, int hoty) {
        std::string path = s_icons_dir + "/" + name + ".svg";
        auto doc = lunasvg::Document::loadFromFile(path);
        if (!doc)
            return nullptr;
        auto bm = doc->renderToBitmap(CURSOR_SIZE, CURSOR_SIZE);
        if (!bm.valid())
            return nullptr;

        // lunasvg produces ARGB premultiplied, matching SDL_PIXELFORMAT_ARGB8888
        SDL_Surface* surf = SDL_CreateSurface(CURSOR_SIZE, CURSOR_SIZE, SDL_PIXELFORMAT_ARGB8888);
        if (!surf)
            return nullptr;
        memcpy(surf->pixels, bm.data(), CURSOR_SIZE * CURSOR_SIZE * 4);
        SDL_Cursor* cursor = SDL_CreateColorCursor(surf, hotx, hoty);
        SDL_DestroySurface(surf);
        return cursor;
    }
}

void cursor_manager::init(const char* icons_dir) {
    s_icons_dir   = icons_dir;
    s_crosshair   = load_cursor("point_scan",   CROSS_HOT_X, CROSS_HOT_Y);
    s_eyedropper  = load_cursor("eyedropper",   EYE_HOT_X,   EYE_HOT_Y);
    s_pan         = load_cursor("pan_tool",     PAN_HOT_X,   PAN_HOT_Y);
    s_pan_pressed = load_cursor("pan_tool_alt", PAN_HOT_X,   PAN_HOT_Y);
}

void cursor_manager::set_for_tool(int tool_index, bool mouse_pressed, bool mouse_over_canvas) {
    if (!mouse_over_canvas) {
        SDL_SetCursor(SDL_GetDefaultCursor());
        return;
    }
    SDL_Cursor* cursor = nullptr;
    switch (tool_index) {
        case 8:  cursor = mouse_pressed ? s_pan_pressed : s_pan; break; // Move
        case 10: cursor = s_eyedropper;                          break; // Color Picker
        default: cursor = s_crosshair;                           break;
    }
    if (cursor)
        SDL_SetCursor(cursor);
    else
        SDL_SetCursor(SDL_GetDefaultCursor());
}

void cursor_manager::shutdown() {
    if (s_crosshair)   { SDL_DestroyCursor(s_crosshair);   s_crosshair   = nullptr; }
    if (s_eyedropper)  { SDL_DestroyCursor(s_eyedropper);  s_eyedropper  = nullptr; }
    if (s_pan)         { SDL_DestroyCursor(s_pan);         s_pan         = nullptr; }
    if (s_pan_pressed) { SDL_DestroyCursor(s_pan_pressed); s_pan_pressed = nullptr; }
}
