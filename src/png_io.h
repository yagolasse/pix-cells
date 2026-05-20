#pragma once
#include <string>
#include "canvas.h"
#include "app_state.h"

namespace png_io {
    bool save(const Canvas& canvas, const std::string& path);
    bool load(Canvas& canvas,       const std::string& path);

    enum class SheetLayout { Horizontal, Vertical, Grid };

    bool save_sprite_sheet(const CanvasState& cs,
                           const std::string& path,
                           SheetLayout layout = SheetLayout::Horizontal,
                           int grid_cols = 4);
}
