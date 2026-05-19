Run the mandatory post-change verification for pix-cells. There is no test suite — the build is the verification.

Run:
```bash
cmake --build build
```

The build must succeed with zero errors. Compiler warnings are also treated as failures — flag any that appear.

If the build directory does not exist yet, configure first:
```bash
cmake -B build && cmake --build build
```

Report a single summary line at the end: build passed or failed (and which file/line if failed).
