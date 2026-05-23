#include "icon_manager.h"
#include <SDL3/SDL_opengl.h>
#include <lunasvg.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

namespace {
    constexpr int ICON_SIZE = 32;
    std::string s_icons_dir;
    std::unordered_map<std::string, GLuint> s_cache;

    GLuint upload_bitmap(lunasvg::Bitmap& bm) {
        int w = bm.width(), h = bm.height();
        const uint8_t* src = bm.data();
        std::vector<uint32_t> pixels(w * h);

        for (int i = 0; i < w * h; i++) {
            // lunasvg stores ARGB premultiplied (host order): A=bits31-24, R=23-16, G=15-8, B=7-0
            uint32_t p = reinterpret_cast<const uint32_t*>(src)[i];
            uint8_t a  = (p >> 24) & 0xFF;
            uint8_t r  = (p >> 16) & 0xFF;
            uint8_t g  = (p >>  8) & 0xFF;
            uint8_t b  = (p      ) & 0xFF;
            // un-premultiply
            if (a > 0 && a < 255) {
                r = (uint32_t(r) * 255 + a / 2) / a;
                g = (uint32_t(g) * 255 + a / 2) / a;
                b = (uint32_t(b) * 255 + a / 2) / a;
            }
            // pack as RGBA8 (R in bits 0-7) to match GL_RGBA / GL_UNSIGNED_BYTE
            pixels[i] = uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24);
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return tex;
    }
}

void icon_manager::init(const char* icons_dir) {
    s_icons_dir = icons_dir;
}

ImTextureID icon_manager::get(const char* name) {
    auto it = s_cache.find(name);
    if (it != s_cache.end())
        return (ImTextureID)(uint64_t)it->second;

    auto doc = lunasvg::Document::loadFromFile(s_icons_dir + "/" + name + ".svg");
    if (!doc) {
        s_cache[name] = 0;
        return (ImTextureID)0;
    }
    auto bm = doc->renderToBitmap(ICON_SIZE, ICON_SIZE);
    if (!bm.valid()) {
        s_cache[name] = 0;
        return (ImTextureID)0;
    }
    GLuint tex = upload_bitmap(bm);
    s_cache[name] = tex;
    return (ImTextureID)(uint64_t)tex;
}

void icon_manager::shutdown() {
    for (auto& [name, tex] : s_cache)
        if (tex != 0)
            glDeleteTextures(1, &tex);
    s_cache.clear();
}
