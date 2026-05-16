#pragma once
#include <string>
#include "canvas.h"

namespace png_io {
    bool save(const Canvas& canvas, const std::string& path);
    bool load(Canvas& canvas,       const std::string& path);
}
