#pragma once
#include "app_state.h"
#include <string>

namespace pixc_io {
bool save(const AppState& state, const std::string& path);
bool load(AppState& state, const std::string& path);
} // namespace pixc_io
