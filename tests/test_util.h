#pragma once
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>

// Shared lightweight test harness (no external framework).
// Each test executable defines bool-returning test_*() functions and runs them
// through a TestRunner in its own main().

inline std::string tmp_path(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

struct TestRunner {
    int failed = 0;
    void run(const char* name, bool (*fn)()) {
        if (fn()) printf("PASS: %s\n", name);
        else    { printf("FAIL: %s\n", name); failed++; }
    }
    int result() const { return failed ? 1 : 0; }
};

// Pixel helpers — RGBA8 with R in bits 0-7 (0xAABBGGRR).
inline uint32_t rgba(int r, int g, int b, int a) {
    return (uint32_t)(((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF));
}
inline int cR(uint32_t p) { return (int)(p & 0xFF); }
inline int cG(uint32_t p) { return (int)((p >> 8) & 0xFF); }
inline int cB(uint32_t p) { return (int)((p >> 16) & 0xFF); }
inline int cA(uint32_t p) { return (int)((p >> 24) & 0xFF); }

inline bool approx(int got, int want, int tol = 1) { return std::abs(got - want) <= tol; }

// Common opaque colors
constexpr uint32_t RED   = 0xFF0000FFu;
constexpr uint32_t GREEN = 0xFF00FF00u;
constexpr uint32_t BLUE  = 0xFFFF0000u;
constexpr uint32_t WHITE = 0xFFFFFFFFu;
constexpr uint32_t BLACK = 0xFF000000u;
constexpr uint32_t CLEAR = 0x00000000u;
