#define _GNU_SOURCE

#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[noreturn]]
void specs_die(
  const c8 *fmt, ...
) {
  va_list args;
  fputs(ERR_PREFIX, stderr);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}

void arena_init(
  Arena *arena, u8 *backing, usize cap
) {
  if (UNLIKELY(!arena || !backing || cap == 0)) specs_die("arena_init: invalid backing");
  arena->base = backing;
  arena->cap  = cap;
  arena->used = 0;
}

void arena_reset(
  Arena *arena
) {
  if (UNLIKELY(!arena)) return;
  arena->used = 0;
}

static usize align_up(
  usize value, usize align
) {
  if (align <= 1) return value;
  return (value + (align - 1)) & ~(align - 1);
}

void *arena_alloc(
  Arena *ATTR_RESTRICT arena, usize size, usize align
) {
  if (UNLIKELY(!arena->base)) specs_die("arena_alloc: uninitialized arena");
  if (size == 0) size = 1;
  if (align == 0) align = 1;
  usize offset = align_up(arena->used, align);
  if (UNLIKELY(offset + size > arena->cap)) specs_die("out of memory");
  void *block = arena->base + offset;
  arena->used = offset + size;
  return block;
}

c8 *arena_copy_str(
  Arena *arena, const c8 *text
) {
  if (!text || !text[0]) {
    c8 *empty = arena_alloc(arena, 1, 1);
    empty[0]  = '\0';
    return empty;
  }
  return arena_copy_n(arena, text, strlen(text));
}

c8 *arena_copy_n(
  Arena *arena, const c8 *text, usize len
) {
  c8 *copy = arena_alloc(arena, len + 1, 1);
  if (len) COPY_MEMORY(copy, text, len);
  copy[len] = '\0';
  return copy;
}

void buf_init(
  Buf *buf, Arena *arena
) {
  buf->arena   = arena;
  buf->cap     = BUF_INITIAL_CAP;
  buf->len     = 0;
  buf->data    = arena_alloc(arena, buf->cap, 1);
  buf->data[0] = '\0';
}

static void buf_reserve(
  Buf *buf, usize extra
) {
  usize need = buf->len + extra + 1;
  if (LIKELY(need <= buf->cap)) return;
  usize cap = buf->cap;
  while (cap < need)
    cap = cap < 1024 ? cap * 2 : cap + (cap / 2);
  c8 *next = arena_alloc(buf->arena, cap, 1);
  COPY_MEMORY(next, buf->data, buf->len + 1);
  buf->data = next;
  buf->cap  = cap;
}

void buf_append_n(
  Buf *buf, const c8 *text, usize len
) {
  if (UNLIKELY(!text || len == 0)) return;
  buf_reserve(buf, len);
  COPY_MEMORY(buf->data + buf->len, text, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
}

void buf_append(
  Buf *buf, const c8 *text
) {
  if (UNLIKELY(!text)) return;
  buf_append_n(buf, text, strlen(text));
}

void buf_append_char(
  Buf *buf, c8 ch
) {
  buf_reserve(buf, 1);
  buf->data[buf->len++] = ch;
  buf->data[buf->len]   = '\0';
}

void buf_printf(
  Buf *buf, const c8 *fmt, ...
) {
  va_list args;
  va_start(args, fmt);
  i32 need = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);
  if (UNLIKELY(need < 0)) specs_die("could not format output");
  buf_reserve(buf, (usize)need);
  va_start(args, fmt);
  vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, args);
  va_end(args);
  buf->len += (usize)need;
}
