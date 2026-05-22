#pragma once
#include "app_state.h"
#include <string>

namespace palette_io {
bool load_hex(PaletteState& pal, const std::string& path);
bool save_hex(const PaletteState& pal, const std::string& path);
bool load_gpl(PaletteState& pal, const std::string& path);
bool save_gpl(const PaletteState& pal, const std::string& path);
} // namespace palette_io
