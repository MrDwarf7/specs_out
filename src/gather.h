#ifndef SPECS_GATHER_H
#define SPECS_GATHER_H

#include "arena.h"
#include "prelude.h"

/*
 * ## Summary
 * Initialize PATH/dirfd caches and the process-wide document arena.
 *
 */
void specs_runtime_init(void);

/*
 * ## Summary
 * Release cached directory file descriptors.
 *
 */
void specs_runtime_fini(void);

/*
 * ## Summary
 * Process-wide document arena (BSS-backed). Valid after specs_runtime_init.
 *
 * ## Returns
 * Pointer to the global arena.
 *
 */
Arena *specs_doc_arena(void);

/*
 * ## Summary
 * Capture dumps (optionally in parallel) and format the specs document.
 *
 * ## Returns
 * Arena-owned document text.
 *
 */
[[nodiscard]]
c8 *specs_gather_document(Arena *arena);

#endif /* SPECS_GATHER_H */
