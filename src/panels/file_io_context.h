#pragma once
#include "png_io.h"
#include <string>

// Canvas / project file I/O (used by menu_bar.cpp)
enum class IOKind : uint8_t { PixcSave, PixcOpen, PngExport, SpriteSheet };

struct PendingIO {
    bool        active          = false;
    IOKind      kind            = IOKind::PixcSave;
    std::string path;
    int         doc_idx         = 0;
    bool        close_doc_after = false;
    png_io::SheetLayout sheet_layout = png_io::SheetLayout::Horizontal;
    int                 sheet_cols   = 4;
};

// Palette file I/O (used by palette_panel.cpp)
enum class PalIOKind : uint8_t { Import, ExportGPL, ExportHEX };

struct PalPendingIO {
    bool        active = false;
    PalIOKind   kind   = PalIOKind::Import;
    std::string path;
};
