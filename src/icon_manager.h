#pragma once
#include <imgui.h>

namespace icon_manager {
    void init(const char* icons_dir);
    ImTextureID get(const char* name);
    void shutdown();
}
