#ifndef SPECS_ARENA_H
#define SPECS_ARENA_H

#include "constants.h"
#include "prelude.h"

#include <stdarg.h>

/*
 * ## Summary
 * Bump allocator over a caller-owned backing store. No per-object free.
 *
 */
typedef struct {
  u8   *base;
  usize cap;
  usize used;
} Arena;

/*
 * ## Summary
 * Arena-backed growable string. Growth allocates a larger bump block and copies.
 *
 */
typedef struct {
  Arena *arena;
  c8    *data;
  usize  len;
  usize  cap;
} Buf;

/*
 * ## Summary
 * Print `[ERR] ` + formatted message to stderr and exit.
 *
 */
[[noreturn]]
ATTR_COLD ATTR_NONNULL(1) void specs_die(const c8 *fmt, ...);

/*
 * ## Summary
 * Attach `backing[0..cap)` as the arena storage and reset used to 0.
 *
 */
void arena_init(Arena *arena, u8 *backing, usize cap);

/*
 * ## Summary
 * Discard all allocations by setting used to 0. Pointers previously returned
 * become invalid.
 *
 */
void arena_reset(Arena *arena);

/*
 * ## Summary
 * Bump-allocate `size` bytes with `align`. Dies on exhaustion.
 *
 * ## Returns
 * Pointer into the arena backing store.
 *
 */
[[nodiscard]]
ATTR_NONNULL(1) ATTR_ALLOC_SIZE(2) ATTR_RETURNS_NONNULL void *arena_alloc(Arena *ATTR_RESTRICT arena, usize size, usize align);

/*
 * ## Summary
 * Copy a NUL-terminated string into the arena.
 *
 * ## Returns
 * Arena-owned copy, or an interned empty string when `text` is null/empty.
 *
 */
[[nodiscard]]
c8 *arena_copy_str(Arena *arena, const c8 *text);

/*
 * ## Summary
 * Copy `len` bytes and NUL-terminate.
 *
 * ## Returns
 * Arena-owned copy.
 *
 */
[[nodiscard]]
c8 *arena_copy_n(Arena *arena, const c8 *text, usize len);

/*
 * ## Summary
 * Initialize `buf` with a small arena-backed block.
 *
 */
void buf_init(Buf *buf, Arena *arena);

/*
 * ## Summary
 * Append `len` bytes from `text`.
 *
 */
void buf_append_n(Buf *buf, const c8 *text, usize len);

/*
 * ## Summary
 * Append a NUL-terminated string.
 *
 */
void buf_append(Buf *buf, const c8 *text);

/*
 * ## Summary
 * Append a single character.
 *
 */
void buf_append_char(Buf *buf, c8 ch);

/*
 * ## Summary
 * printf-style append into the arena-backed buffer.
 *
 */
void buf_printf(Buf *buf, const c8 *fmt, ...);

#endif /* SPECS_ARENA_H */
