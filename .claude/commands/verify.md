Run the mandatory post-change verification for pix-cells.

Run:
```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

If the build directory does not exist yet, configure first:
```bash
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Both steps must pass:
1. **Build** — zero errors, zero warnings (warnings are treated as failures).
2. **Tests** — all `ctest` tests pass. Currently: `test_pixc` (PIXC round-trip save/load).

Report a single summary line at the end: build+tests passed or failed (and which file/line/test if failed).
