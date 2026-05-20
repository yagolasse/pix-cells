#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#pragma GCC diagnostic pop
#include "png_io.h"
#include <cstring>

bool png_io::save(const Canvas& c, const std::string& path) {
    return stbi_write_png(path.c_str(), c.width, c.height, 4, c.pixels.data(), c.width * 4) != 0;
}

bool png_io::load(Canvas& c, const std::string& path) {
    int w, h, ch;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data)
        return false;
    c.resize(w, h);
    std::memcpy(c.pixels.data(), data, w * h * 4);
    stbi_image_free(data);
    return true;
}
