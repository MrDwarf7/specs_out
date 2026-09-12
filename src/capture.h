#ifndef SPECS_CAPTURE_H
#define SPECS_CAPTURE_H

#include "arena.h"
#include "prelude.h"

/*
 * ## Summary
 * One-time PATH tokenize, PATH dirfds, and /proc /sys /etc dirfds.
 *
 */
void capture_init(void);

/*
 * ## Summary
 * Close cached directory file descriptors.
 *
 */
void capture_fini(void);

/*
 * ## Summary
 * Resolve `name` via the cached PATH using faccessat on cached dirfds.
 *
 * ## Returns
 * Full path if executable,
 * `nullptr` if not found.
 *
 */
const c8 *capture_resolve(const c8 *name);

/*
 * ## Summary
 * Spawn `argv` with posix_spawn, capture stdout into the arena, stderr to /dev/null.
 *
 * ## Returns
 * `true` on success (or allowed failure, yielding empty output),
 * `false` on required-command failure.
 *
 */
ATTR_HOT
bool capture_argv(Arena *arena, const c8 *const *argv, bool allow_failure, c8 **out);

/*
 * ## Summary
 * Read a filesystem path into the arena (empty string on open/read failure).
 *
 * ## Returns
 * Arena-owned file contents (possibly empty).
 *
 */
[[nodiscard]]
c8 *capture_read_file(Arena *arena, const c8 *path);

#endif /* SPECS_CAPTURE_H */
