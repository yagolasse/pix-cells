#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace {
constexpr auto make_u8f() {
    std::array<float, 256> t{};
    for (int i = 0; i < 256; ++i) t[i] = static_cast<float>(i) / 255.0f;
    return t;
}
constexpr auto k_u8f = make_u8f();
} // namespace

// Composite src over dst with blend mode and pre-applied opacity — RGBA8, R in bits 0-7
inline uint32_t blend_pixel(uint32_t dst, uint32_t src, uint8_t mode) {
    float sa = k_u8f[(src >> 24) & 0xFF];
    if (sa <= 0.0f) return dst;
    float sr = k_u8f[src        & 0xFF];
    float sg = k_u8f[(src >>  8) & 0xFF];
    float sb = k_u8f[(src >> 16) & 0xFF];
    float da = k_u8f[(dst >> 24) & 0xFF];
    float dr = k_u8f[dst        & 0xFF];
    float dg = k_u8f[(dst >>  8) & 0xFF];
    float db = k_u8f[(dst >> 16) & 0xFF];

    float br = sr, bg = sg, bb = sb;
    switch (mode) {
    case 1: br=sr*dr;                bg=sg*dg;                bb=sb*db;                break; // Multiply
    case 2: br=1-(1-sr)*(1-dr);      bg=1-(1-sg)*(1-dg);      bb=1-(1-sb)*(1-db);      break; // Screen
    case 3: br=dr<.5f?(2.0f*sr*dr):(1.0f-(2.0f*(1.0f-sr)*(1.0f-dr)));                        // Overlay
            bg=dg<.5f?(2.0f*sg*dg):(1.0f-(2.0f*(1.0f-sg)*(1.0f-dg)));
            bb=db<.5f?(2.0f*sb*db):(1.0f-(2.0f*(1.0f-sb)*(1.0f-db)));            break;
    case 4: br=std::min(sr+dr,1.f);  bg=std::min(sg+dg,1.f);  bb=std::min(sb+db,1.f);  break; // Add
    default: break; // Normal
    }
    float oa  = sa + (da * (1.0f - sa));
    if (oa == 0.0f) return 0;
    float or_ = (br*sa*da + sr*sa*(1.0f-da) + dr*da*(1.0f-sa)) / oa;
    float og  = (bg*sa*da + sg*sa*(1.0f-da) + dg*da*(1.0f-sa)) / oa;
    float ob  = (bb*sa*da + sb*sa*(1.0f-da) + db*da*(1.0f-sa)) / oa;
    return ((uint32_t)(oa *255.0f)<<24)
         | ((uint32_t)(ob *255.0f)<<16)
         | ((uint32_t)(og *255.0f)<< 8)
         |  (uint32_t)(or_*255.0f);
}
