#include "app_state.h"

static ImVec4 hex_to_imvec4(uint32_t rgb) {
    return {
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >>  8) & 0xFF) / 255.0f,
        ( rgb        & 0xFF) / 255.0f,
        1.0f
    };
}

PaletteState::PaletteState() {
    static const uint32_t pico8[24] = {
        0x1a1a1a, 0x3a3a3a, 0x5a5a5a, 0x7e7e7e, 0xa5a5a5, 0xcccccc, 0xf6efe0, 0xffffff,
        0xc84638, 0xe07c52, 0xe7c693, 0xf3e8a8, 0xa8c97a, 0x5aa162, 0x3a6dc8, 0x243a7a,
        0xa45cc4, 0x6a3e9a, 0x2e9aa6, 0x194b56, 0x7a4a2a, 0xcc7a3a, 0xd44a7a, 0x84324f,
    };
    for (auto h : pico8) swatches.push_back(hex_to_imvec4(h));

    primary_color   = swatches[6]; // cream (#F6EFE0)
    secondary_color = swatches[7]; // white (#FFFFFF)
    selected_swatch = 6;

    for (int i = 0; i < 8; i++) recent_colors.push_back(swatches[i]);
}
