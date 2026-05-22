#include "palette_io.h"
#include "log.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

// Strip trailing \r and \n from a C string in place.
static void strip_newline(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

// Filename without extension, used as palette name fallback on import.
static std::string stem_from_path(const std::string& path) {
    auto sep = path.find_last_of("/\\");
    std::string name = (sep == std::string::npos) ? path : path.substr(sep + 1);
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

static bool all_hex(const char* s, int len) {
    for (int i = 0; i < len; i++)
        if (!isxdigit((unsigned char)s[i]))
            return false;
    return true;
}

static int to_channel(float v) {
    return (int)(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

// ── HEX ──────────────────────────────────────────────────────────────────────

bool palette_io::load_hex(PaletteState& pal, const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        Log("palette_io::load_hex: cannot open \"%s\"", path.c_str());
        return false;
    }

    std::vector<ImVec4> colors;
    std::string         name;
    bool                first_line = true;
    char                line[256];

    while (fgets(line, sizeof(line), f)) {
        strip_newline(line);

        // Trim leading whitespace.
        const char* p = line;
        while (*p == ' ' || *p == '\t') ++p;

        if (*p == '\0')
            continue;

        // Comment / name line.
        if (p[0] == ';' || (p[0] == '/' && p[1] == '/')) {
            if (first_line) {
                // Extract name from "; Name" or "// Name".
                const char* n = p + (p[1] == '/' ? 2 : 1);
                while (*n == ' ' || *n == '\t') ++n;
                if (*n) name = n;
            }
            first_line = false;
            continue;
        }
        first_line = false;

        // Strip optional '#' prefix.
        if (*p == '#') ++p;

        if (strlen(p) == 6 && all_hex(p, 6)) {
            unsigned int val = 0;
            sscanf(p, "%6X", &val);
            int r = (val >> 16) & 0xFF;
            int g = (val >> 8) & 0xFF;
            int b = val & 0xFF;
            colors.push_back({r / 255.0f, g / 255.0f, b / 255.0f, 1.0f});
        }
        // Non-matching lines are silently skipped.
    }
    fclose(f);

    if (colors.empty()) {
        Log("palette_io::load_hex: no colors in \"%s\"", path.c_str());
        return false;
    }

    pal.swatches        = std::move(colors);
    pal.selected_swatch = -1;
    pal.palette_name    = name.empty() ? stem_from_path(path) : name;
    Log("palette_io::load_hex: loaded %d colors from \"%s\"", (int)pal.swatches.size(), path.c_str());
    return true;
}

bool palette_io::save_hex(const PaletteState& pal, const std::string& path) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        Log("palette_io::save_hex: cannot open \"%s\"", path.c_str());
        return false;
    }

    fprintf(f, "; %s\n", pal.palette_name.c_str());
    for (const auto& c : pal.swatches)
        fprintf(f, "#%02X%02X%02X\n", to_channel(c.x), to_channel(c.y), to_channel(c.z));

    fclose(f);
    Log("palette_io::save_hex: saved %d colors to \"%s\"", (int)pal.swatches.size(), path.c_str());
    return true;
}

// ── GPL ──────────────────────────────────────────────────────────────────────

bool palette_io::load_gpl(PaletteState& pal, const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        Log("palette_io::load_gpl: cannot open \"%s\"", path.c_str());
        return false;
    }

    char line[512];

    // First line must be the format signature.
    if (!fgets(line, sizeof(line), f)) {
        Log("palette_io::load_gpl: empty file \"%s\"", path.c_str());
        fclose(f);
        return false;
    }
    strip_newline(line);
    if (strcmp(line, "GIMP Palette") != 0) {
        Log("palette_io::load_gpl: bad magic in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }

    // Scan header lines until we hit the '#' separator.
    std::string name;
    bool        in_colors = false;
    while (fgets(line, sizeof(line), f)) {
        strip_newline(line);
        if (line[0] == '#') {
            in_colors = true;
            break;
        }
        if (strncmp(line, "Name:", 5) == 0) {
            const char* n = line + 5;
            while (*n == ' ' || *n == '\t') ++n;
            name = n;
        }
        // Columns: and other header lines are skipped.
    }

    if (!in_colors) {
        Log("palette_io::load_gpl: no '#' separator in \"%s\"", path.c_str());
        fclose(f);
        return false;
    }

    std::vector<ImVec4> colors;
    while (fgets(line, sizeof(line), f)) {
        strip_newline(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        int r = 0, g = 0, b = 0;
        if (sscanf(line, "%d %d %d", &r, &g, &b) == 3) {
            colors.push_back({
                std::clamp(r, 0, 255) / 255.0f,
                std::clamp(g, 0, 255) / 255.0f,
                std::clamp(b, 0, 255) / 255.0f,
                1.0f});
        }
    }
    fclose(f);

    if (colors.empty()) {
        Log("palette_io::load_gpl: no colors in \"%s\"", path.c_str());
        return false;
    }

    pal.swatches        = std::move(colors);
    pal.selected_swatch = -1;
    pal.palette_name    = name.empty() ? stem_from_path(path) : name;
    Log("palette_io::load_gpl: loaded %d colors from \"%s\"", (int)pal.swatches.size(), path.c_str());
    return true;
}

bool palette_io::save_gpl(const PaletteState& pal, const std::string& path) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        Log("palette_io::save_gpl: cannot open \"%s\"", path.c_str());
        return false;
    }

    fprintf(f, "GIMP Palette\n");
    fprintf(f, "Name: %s\n", pal.palette_name.c_str());
    fprintf(f, "Columns: 8\n");
    fprintf(f, "#\n");
    for (const auto& c : pal.swatches)
        fprintf(f, "%3d %3d %3d\tUntitled\n",
                to_channel(c.x), to_channel(c.y), to_channel(c.z));

    fclose(f);
    Log("palette_io::save_gpl: saved %d colors to \"%s\"", (int)pal.swatches.size(), path.c_str());
    return true;
}
