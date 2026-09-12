#ifndef SPECS_FORMAT_H
#define SPECS_FORMAT_H

#include "parse.h"
#include "prelude.h"

/*
 * ## Summary
 * Format CPU facts into the document fragment after `CPU: `.
 *
 * ## Returns
 * Arena-owned fragment.
 *
 */
[[nodiscard]]
c8 *format_cpu(Arena *arena, const c8 *lscpu, const c8 *smt, i64 nproc);

/*
 * ## Summary
 * Format RAM from `/proc/meminfo` plus optional `inxi -m` text.
 *
 * ## Returns
 * Arena-owned fragment.
 *
 */
[[nodiscard]]
c8 *format_ram(Arena *arena, const c8 *meminfo, const c8 *inxi);

/*
 * ## Summary
 * Format GPU from nvidia-smi csv (preferred) or lspci VGA lines.
 *
 * ## Returns
 * Arena-owned fragment (`unknown` when neither source yields a name).
 *
 */
[[nodiscard]]
c8 *format_gpu(Arena *arena, const c8 *nvidia_smi, const c8 *lspci);

/*
 * ## Summary
 * Format motherboard as `vendor name`, or `unknown` when both empty.
 *
 * ## Returns
 * Arena-owned fragment.
 *
 */
[[nodiscard]]
c8 *format_mobo(Arena *arena, const c8 *vendor, const c8 *name);

/*
 * ## Summary
 * Format OS as `NAME, kernel KVER`. Missing NAME becomes `Linux`.
 *
 * ## Returns
 * Arena-owned fragment.
 *
 */
[[nodiscard]]
c8 *format_os(Arena *arena, const c8 *os_release, const c8 *kernel);

/*
 * ## Summary
 * Format network grouping from an `ip -br link` dump.
 *
 * ## Returns
 * Arena-owned fragment (`none` when no non-lo interfaces).
 *
 */
[[nodiscard]]
c8 *format_network(Arena *arena, const c8 *ip_br_link);

/*
 * ## Summary
 * Format swap bits from `/proc/swaps`, an lsblk table, and optional file hints.
 *
 * ## Returns
 * Arena-owned fragment (`none` when nothing is present).
 *
 */
[[nodiscard]]
c8 *format_swap(Arena *arena, const c8 *swaps, const LsblkTable *table, const SwapFileHint *files, usize file_count);

/*
 * ## Summary
 * Format disk groups (boot, then NVMe, then storage) from a parsed lsblk table.
 *
 * ## Returns
 * Arena-owned multi-line fragment (empty when no disks).
 *
 */
[[nodiscard]]
c8 *format_disks(Arena *arena, const LsblkTable *table, const c8 *boot_disk);

/*
 * ## Summary
 * Assemble the full specs document in original section order.
 *
 * ## Returns
 * Arena-owned document including trailing newline on the last section.
 *
 */
[[nodiscard]]
c8 *specs_build_doc(
  Arena *arena, const c8 *cpu, const c8 *ram, const c8 *gpu, const c8 *mobo, const c8 *disks, const c8 *swap, const c8 *network, const c8 *os
);

#endif /* SPECS_FORMAT_H */
