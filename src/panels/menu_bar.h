#pragma once
#include <SDL3/SDL.h>
#include "app_state.h"

namespace panels {
    // Returns false when the user chooses File > Quit.
    bool DrawMenuBar(AppState& state, SDL_Window* window);
}
