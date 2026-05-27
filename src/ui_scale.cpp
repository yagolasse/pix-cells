#include "ui_scale.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cmath>
#include <string>

namespace ui_scale {

static constexpr float kScales[5] = {0.75f, 1.0f, 1.25f, 1.5f, 2.0f};

static ImFont*     s_fonts[5] = {};
static ImGuiStyle  s_base_style;
static float       s_scale = 1.0f;

void init(ImFont* fonts[5], const ImGuiStyle& base_style) {
    for (int i = 0; i < 5; ++i)
        s_fonts[i] = fonts[i];
    s_base_style = base_style;
}

void apply(float scale) {
    int best = 1;
    float best_diff = 99.f;
    for (int i = 0; i < 5; ++i) {
        float d = std::abs(scale - kScales[i]);
        if (d < best_diff) { best_diff = d; best = i; }
    }
    s_scale = kScales[best];
    ImGui::GetIO().FontDefault = s_fonts[best];
    ImGui::GetStyle() = s_base_style;
    if (s_scale != 1.0f)
        ImGui::GetStyle().ScaleAllSizes(s_scale);
}

float get() { return s_scale; }

float px(float base) { return base * s_scale; }

static std::string pref_path() {
    char* p = SDL_GetPrefPath("pix-cells", "pix-cells");
    if (!p) return {};
    std::string result(p);
    SDL_free(p);
    return result + "settings.ini";
}

void save() {
    std::string path = pref_path();
    if (path.empty()) return;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
    fprintf(f, "ui_scale=%.2f\n", s_scale);
    fclose(f);
}

float load() {
    std::string path = pref_path();
    if (path.empty()) return 1.0f;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return 1.0f;
    float v = 1.0f;
    fscanf(f, "ui_scale=%f", &v);
    fclose(f);
    return v;
}

} // namespace ui_scale
