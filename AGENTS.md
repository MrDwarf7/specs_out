# AGENTS.md

Agent guide for the `specs_out` project.

**Read this first.** Then refer to docs/ only as needed for specific topics.

---

## Project Overview

specs_out is a C project experimenting with Rust-like language features -- Result types, pattern matching, ownership semantics, and similar ergonomic improvements on top of standard C23.

---

## Build & Test

```sh
make cl && make t          # clean build + run all tests
make test                  # run all tests (shorthand: make t)
make b                     # debug build
make r                     # debug build + run
make a                     # debug + release + test
```

- **Debug builds** output to `build/`. **Release builds** output to `build_rel/`. They coexist.
- `make b` / `make r` -- debug build / run. `make br` / `make rr` -- release build / run.
- Single test entry point: `test/test_main.c` -- routes by suite name argument.
- Object files for tests live in `$(BUILD_DIR)/test_obj/` (separate from main build).
- Zero warnings is a hard gate. `-Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion`.
- C standard: C23 (`-std=gnu23` on GCC 14+, `-std=gnu2x` fallback).

---

## Project Structure

```
src/            # Production source (prelude.h, defer.h, main.c, ...)
test/           # Test suites (one per concern) + test_framework.h + test_main.c
libs/           # Vendored deps (relaxed warnings)
docker/         # Dockerfile + docker-compose.yml
scripts/        # Dev/test scripts (dev-start.sh)
```

---

## Documentation Conventions

Doc comments use Rust-style `/* */` blocks placed **directly** above the declaration (no empty line between).

### Where docs go

- **Header files (`.h`)**: Document **public** API only.
- **Source files (`.c`)**: Document **private** (static) functions and internal helpers.
- **No duplicate docs.** If a function is declared in a header and defined in a `.c` file, the doc block goes in the header **only**.

### Format

```c
/*
 * ## Summary
 * One or more lines describing what the function does.
 *
 * ## Returns
 * `true` on success,
 * `false` on failure.
 *
 */
[[nodiscard]]
bool some_function(const u8 *buf, usize len);
```

### Rules

1. **Opening line**: `/*` on its own line.
2. **Summary header**: `* ## Summary` -- level-2 markdown.
3. **Returns header** (functions only): `* ## Returns` -- each return case on its own `*` line.
4. **Closing**: `*\n */` -- trailing space-star.
5. **No empty line** between doc block and declaration.
6. **Structs/enums**: Use `## Summary` only. Inline `/* ... */` for individual fields.
7. **Macros**: Use `/*` block comments above the `#define`.

---

## Type Conventions

Use Rust-inspired type aliases from `src/prelude.h`. **Never** use `stdint.h` types directly in new code.

| Use                       | Instead of                 | Notes                 |
| ------------------------- | -------------------------- | --------------------- |
| `u8`, `u16`, `u32`, `u64` | `uint8_t`, `uint16_t`, ... | Unsigned integers     |
| `i8`, `i16`, `i32`, `i64` | `int8_t`, `int16_t`, ...   | Signed integers       |
| `f32`, `f64`              | `float`, `double`          | Floating point        |
| `usize`                   | `size_t`                   | Size/length values    |
| `bool`                    | `int` (for truthiness)     | C23 bool or stdbool.h |
| `c8`, `uc8`               | `char`, `unsigned char`    | Character types       |
| `byte`, `word`, `dword`   | `u8`, `u16`, `u32`         | Semantic aliases      |

For types that don't have a direct alias (e.g., `sig_atomic_t`, `pthread_t`), use the standard C type.

### Semantic typedefs

When a generic type is used in a specific, universal role, create a named typedef for clarity:

```c
typedef u8 slot_idx_t;  // index into a backing store slot
```

---

## Safety Patterns & Idioms

### 1. Indices over raw pointers
Use index-based access into backing stores rather than handing out raw pointers across boundaries.

### 2. Pool allocation over heap in hot path
Static pools with bitmaps over `malloc` in the data path. Intentional backpressure: pool full -> drop.

### 3. `[[nodiscard]]` on alloc functions
Functions that return ownership must be marked `[[nodiscard]]`.

### 4. Bounds validation at config load + defense-in-depth at runtime
Validate at startup, re-check at runtime as defense-in-depth.

### 5. Lock-free data structures
SPSC with `_Atomic` and `memory_order_*`. Cache-line padded head/tail to prevent false sharing.

---

## Test Conventions

- **Single entry point**: `test/test_main.c` with suite routing via `argv[1]`.
- **Framework**: Custom macros in `test/test_framework.h`. No external test framework.
- **Shared counters**: `u32 run, passed` defined in `test_main.c` only.
- **File naming**: `test/test_*.c` -- all test files must follow this prefix.
- **Self-containment**: Each test file should be compilable standalone given the right include paths.
- **Assertions**: Use `TEST("name", expr)` for individual checks. Use `TEST_ASSERT_*` for typed checks. Use `BATCH_CHECK` / `BATCH_RESULT` for loop-based tests.
- **Discard warnings**: Cast `[[nodiscard]]` returns with `(void)` when intentionally ignoring.

---

## Code Style

- **Formatting**: clang-format with project config (`.clang-format`).
- **Include order**: project headers -> system headers -> blank line.
- **No unicode dividers** in source code.
- **Preserve TODO/PERF/NOTE/FIXME markers** -- they document limitations, not dead code.
- **`nullptr`** over `NULL` or `0` for null pointers (C23).
- **`ZERO_MEMORY` / `COPY_MEMORY`** macros over raw `memset`/`memcpy` where appropriate.
- **Named constants** over magic numbers.
- **Config-driven everything** -- no hardcoded values where they could be configurable.
