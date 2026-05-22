#include "test_util.h"
#include "blend.h"

// blend_pixel(dst, src, mode): mode 0=Normal 1=Multiply 2=Screen 3=Overlay 4=Add.
// Source alpha carries pre-applied opacity. Channels truncate (no rounding),
// so comparisons use approx().

static bool test_opaque_src_replaces() {
    // Opaque src over any dst yields src exactly.
    uint32_t out = blend_pixel(BLUE, RED, 0);
    return out == RED;
}

static bool test_transparent_src_noop() {
    uint32_t out = blend_pixel(RED, CLEAR, 0);
    return out == RED;
}

static bool test_normal_half_alpha() {
    // 50%-alpha white over opaque black ≈ mid grey, fully opaque result.
    uint32_t src = rgba(255, 255, 255, 128);
    uint32_t out = blend_pixel(BLACK, src, 0);
    return approx(cR(out), 128, 2) && approx(cG(out), 128, 2) &&
           approx(cB(out), 128, 2) && cA(out) == 255;
}

static bool test_multiply() {
    // red over white, multiply → red (white is identity for multiply)
    uint32_t out = blend_pixel(WHITE, RED, 1);
    if (!(approx(cR(out), 255) && approx(cG(out), 0) && approx(cB(out), 0)))
        return false;
    // anything over black, multiply → black
    uint32_t out2 = blend_pixel(BLACK, RED, 1);
    return approx(cR(out2), 0) && approx(cG(out2), 0) && approx(cB(out2), 0);
}

static bool test_screen() {
    // s screen black → s
    uint32_t out = blend_pixel(BLACK, RED, 2);
    if (!(approx(cR(out), 255) && approx(cG(out), 0) && approx(cB(out), 0)))
        return false;
    // s screen white → white
    uint32_t out2 = blend_pixel(WHITE, RED, 2);
    return approx(cR(out2), 255) && approx(cG(out2), 255) && approx(cB(out2), 255);
}

static bool test_add() {
    // red + green = yellow
    uint32_t out = blend_pixel(GREEN, RED, 4);
    if (!(approx(cR(out), 255) && approx(cG(out), 255) && approx(cB(out), 0)))
        return false;
    // saturates: white + red = white
    uint32_t out2 = blend_pixel(WHITE, RED, 4);
    return approx(cR(out2), 255) && approx(cG(out2), 255) && approx(cB(out2), 255);
}

static bool test_overlay_both_branches() {
    // Overlay branches on dst<0.5 vs >=0.5, per channel.
    // dst dark (0.25→64) and light (0.75→191), src grey 0.5 (128).
    uint32_t src = rgba(128, 128, 128, 255);
    uint32_t dst = rgba(64, 191, 0, 255);
    uint32_t out = blend_pixel(dst, src, 3);
    // R: dr=0.251<0.5 → 2*sr*dr = 2*0.502*0.251 ≈ 0.252 → ~64
    // G: dg=0.749>=0.5 → 1-2*(1-sr)*(1-dg) = 1-2*0.498*0.251 ≈ 0.75 → ~191
    return approx(cR(out), 64, 4) && approx(cG(out), 191, 4);
}

int main() {
    TestRunner t;
    t.run("opaque_src_replaces",  test_opaque_src_replaces);
    t.run("transparent_src_noop", test_transparent_src_noop);
    t.run("normal_half_alpha",    test_normal_half_alpha);
    t.run("multiply",             test_multiply);
    t.run("screen",               test_screen);
    t.run("add",                  test_add);
    t.run("overlay_both_branches",test_overlay_both_branches);
    return t.result();
}
