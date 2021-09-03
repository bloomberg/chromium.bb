layout(key, ctype=SkPMColor4f) in half4 colorWhite;

// (This test code was largely borrowed from shared/ForLoopControlFlow.sksl.)
half4 main() {
    half4 color = colorWhite;

    // A basic for-loop with no block.
    for (half a = 0; a <= 1; ++a) color.a = a;

    // A for-loop with a block and a break inside.
    for (half r = -5; r < 5; r += 1) {
        color.r = r;
        if (color.r == 0) break;
    }

    // A for-loop with a block and a continue inside.
    for (half b = 5; b >= 0; b -= 1) {
        color.b = b;
        if (color.a == 1) continue; // should always happen
        color.g = 0;
    }

    // A for-loop with two init-variables.
    for (uint x = 0, y = 1; x <= y; ++x) {
        color.a = half(x);
    }

    // color contains green.
    return color;
}
