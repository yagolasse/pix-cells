#pragma once
#include <SDL3/SDL.h>
#include "app_state.h"

namespace panels {
// Returns false when the user confirms quit. quit_requested is set by the SDL window-close event.
bool DrawMenuBar(AppState& state, SDL_Window* window, bool& show_log, bool& quit_requested);
} // namespace panels
