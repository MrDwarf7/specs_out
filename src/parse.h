#ifndef SPECS_PARSE_H
#define SPECS_PARSE_H

#include "arena.h"
#include "prelude.h"

#include <stdalign.h>

/*
 * ## Summary
 * One `lsblk -P` row after KEY="value" extraction (escapes already decoded).
 *
 */
typedef struct {
  const c8 *name;
  const c8 *type;
  const c8 *pkname;
  const c8 *tran;
  const c8 *size;
  const c8 *rota;
  const c8 *parttypename;
  const c8 *fstype;
  const c8 *mountpoint;
} ATTR_ALIGNED(8) LsblkRow;

_Static_assert(sizeof(LsblkRow) == 9 * sizeof(const c8 *), "LsblkRow: 9 pointers, no padding, model dropped");
_Static_assert(alignof(LsblkRow) >= 8, "LsblkRow stays pointer-aligned; packed would unalign the table");

/*
 * ## Summary
 * Parsed lsblk dump. `rows` is arena-owned; count is the populated length.
 *
 */
typedef struct {
  LsblkRow *rows;
  usize     count;
} LsblkTable;

/*
 * ## Summary
 * One `ip -br link` interface after token split.
 *
 */
typedef struct {
  const c8 *name;
  const c8 *state;
} IfaceRow;

/*
 * ## Summary
 * Parsed interface table.
 *
 */
typedef struct {
  IfaceRow *rows;
  usize     count;
} IfaceTable;

/*
 * ## Summary
 * Parsed `/proc/swaps` paths (header skipped). Pointer array, no wrapper struct.
 *
 */
typedef struct {
  const c8 **paths;
  usize      count;
} SwapTable;

/*
 * ## Summary
 * Optional swap-file size discovered via `stat`/`du` (live path) or a fixture.
 *
 */
typedef struct {
  const c8 *path;
  const c8 *size;
} SwapFileHint;

/*
 * ## Summary
 * CPU facts parsed once from an `lscpu` dump plus SMT file and online nproc.
 *
 */
typedef struct {
  const c8 *model;
  f32       boost_ghz;
  i32       cores;
  i32       sockets;
  i32       threads;
  u8        smt_on;
  u8        has_boost;
} ATTR_PACKED_ALIGNED(8) CpuFacts;

_Static_assert(sizeof(CpuFacts) == 32, "CpuFacts packed; aligned(8) keeps model pointer aligned (tail pad 6)");

/*
 * ## Summary
 * Strip ASCII whitespace from both ends into an arena copy.
 *
 * ## Returns
 * Arena-owned trimmed string.
 *
 */
[[nodiscard]]
ATTR_HOT ATTR_NONNULL(1) c8 *trim_copy(Arena *arena, const c8 *text);

/*
 * ## Summary
 * Copy text up to the first newline (newline excluded).
 *
 * ## Returns
 * Arena-owned first line.
 *
 */
[[nodiscard]]
c8 *first_line_copy(Arena *arena, const c8 *text);

/*
 * ## Summary
 * Compact size token and expand a trailing T/G/M/K into ` TB`/` GB`/` MB`/` KB`.
 *
 * ## Returns
 * Arena-owned label (e.g. `512G` -> `512 GB`).
 *
 */
[[nodiscard]]
c8 *size_label(Arena *arena, const c8 *input);

/*
 * ## Summary
 * Extract KEY="value" from one `lsblk -P` line, decoding `\n`/`\t`/other escapes.
 *
 * ## Returns
 * Arena-owned value, or empty string when the key is absent.
 *
 */
[[nodiscard]]
ATTR_HOT ATTR_NONNULL(1) c8 *lsblk_field(Arena *arena, const c8 *line, const c8 *key);

/*
 * ## Summary
 * Return the trimmed rest of the first line that begins with `label`.
 *
 * ## Returns
 * Arena-owned value, or empty string when not found.
 *
 */
[[nodiscard]]
c8 *line_value(Arena *arena, const c8 *text, const c8 *label);

/*
 * ## Summary
 * Return the whitespace-delimited word after `marker` (first match).
 *
 * ## Returns
 * Arena-owned word, or empty string when not found.
 *
 */
[[nodiscard]]
c8 *word_after(Arena *arena, const c8 *text, const c8 *marker);

/*
 * ## Summary
 * Parse the kibibyte integer from a meminfo `Key:` line.
 *
 * ## Returns
 * Parsed integer, or 0 when missing.
 *
 */
i64 meminfo_kib(Arena *arena, const c8 *text, const c8 *key);

/*
 * ## Summary
 * Parse an entire `lsblk -P` dump into rows. One pass; fields extracted per line.
 *
 * ## Returns
 * Table pointing at arena-owned rows (count 0 when `text` is empty).
 *
 */
LsblkTable lsblk_parse(Arena *arena, const c8 *text);

/*
 * ## Summary
 * Parse `ip -br link` into name/state rows. `lo*` is kept here; format skips it.
 *
 * ## Returns
 * Table pointing at arena-owned rows.
 *
 */
IfaceTable iface_parse(Arena *arena, const c8 *text);

/*
 * ## Summary
 * Parse `/proc/swaps`, skipping the header row.
 *
 * ## Returns
 * Table of arena-owned paths.
 *
 */
SwapTable swaps_parse(Arena *arena, const c8 *text);

/*
 * ## Summary
 * Parse lscpu dump + SMT file + online thread count into CPU facts.
 *
 * ## Returns
 * CpuFacts with arena-owned model string.
 *
 */
CpuFacts parse_cpu(Arena *arena, const c8 *lscpu, const c8 *smt, i64 nproc);

/*
 * ## Summary
 * True when `name` is zram/loop/ram (devices the original gatherer skips).
 *
 * ## Returns
 * `true` if the device should be ignored,
 * `false` otherwise.
 *
 */
ATTR_PURE ATTR_HOT bool disk_name_skipped(const c8 *name);

/*
 * ## Summary
 * True when `row` is a child partition of `disk_name`.
 *
 * ## Returns
 * `true` if the row is a partition of that disk,
 * `false` otherwise.
 *
 */
ATTR_PURE ATTR_HOT bool row_is_child(const LsblkRow *row, const c8 *disk_name);

/*
 * ## Summary
 * True when `row` is a whole-disk (or other top-level) device, not a partition.
 *
 * ## Returns
 * `true` for top-level devices,
 * `false` for partitions.
 *
 */
ATTR_PURE ATTR_HOT bool row_is_whole_disk(const LsblkRow *row);

/*
 * ## Summary
 * True when `path` is listed in the parsed `/proc/swaps` table.
 *
 * ## Returns
 * `true` if active,
 * `false` otherwise.
 *
 */
bool swap_path_active(const SwapTable *table, const c8 *path);

/*
 * ## Summary
 * Join `count` strings with `sep`.
 *
 * ## Returns
 * Arena-owned joined string (empty when count is 0).
 *
 */
[[nodiscard]]
c8 *join_strs(Arena *arena, const c8 *const *items, usize count, const c8 *sep);

#endif /* SPECS_PARSE_H */
