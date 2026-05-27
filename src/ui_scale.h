#pragma once
#include "imgui.h"

namespace ui_scale {
    // Call once after the ImGui theme is applied, before ImGui backend init.
    // fonts[0..4] correspond to scales 0.75, 1.0, 1.25, 1.5, 2.0.
    void  init(ImFont* fonts[5], const ImGuiStyle& base_style);

    // Switch to a scale level. Accepts: 0.75, 1.0, 1.25, 1.5, 2.0.
    // Silently clamps to the nearest valid value.
    void  apply(float scale);

    float get();            // current scale factor
    float px(float base);   // base * get() — scale a pixel constant

    void  save();  // persist to SDL_GetPrefPath settings.ini
    float load();  // returns 1.0f if file is missing or unparseable
}
