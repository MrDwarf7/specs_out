/*
 * defer.h -- scope-based cleanup helpers for malloc'd pointers (GCC/Clang only)
 *
 * Provides Rust-Drop-like semantics for heap pointers in C, using the
 * GNU/Clang `cleanup` variable attribute. A function attached via cleanup
 * is invoked automatically when its variable goes out of scope -- on
 * every exit path (normal return, early return, goto, etc.) -- without
 * you having to remember to call free() manually at each one.
 *
 * Two macros are provided:
 *
 *   DEFER_MALLOC(type, name, count)
 *       Declares `type *name`, allocates `count * sizeof(type)` bytes
 *       via malloc(), and attaches automatic cleanup. Use this when you
 *       want allocation + auto-free in one line.
 *
 *   DEFER_FREE(name)
 *       Attaches automatic cleanup to an *already-declared* pointer
 *       variable `name`, without allocating anything. Use this when a
 *       pointer was handed to you by another function (e.g. strdup,
 *       a constructor, a library call) and you just want it freed
 *       automatically at scope exit.
 *
 * Both rely on a single generic cleanup function, free_generic(), which
 * takes `void *` rather than a concrete pointer type. This is necessary
 * because the `cleanup` attribute type-checks its function argument
 * against the address of the variable it's attached to -- a cleanup
 * function typed for `uint8_t **` will trigger
 * -Wincompatible-pointer-types if attached to an `int *` variable.
 * Using `void *` sidesteps this: any pointer-to-pointer converts to
 * void* without a diagnostic, so one cleanup function serves every type.
 *
 * CAVEATS:
 *   - `cleanup` is a GCC/Clang extension, not standard C (not even C23).
 *     No equivalent exists in MSVC; #ifdef around these macros or accept
 *     manual free() on that compiler.
 *   - Only valid on pointers that are either NULL or were heap-allocated
 *     in a way that plain free() is correct for (malloc/calloc/realloc/
 *     strdup, etc.). Don't attach this to pointers owned by something
 *     else (e.g. pointers into a buffer you don't own) -- you'll double
 *     free or free memory you don't own.
 *   - Setting the pointer to NULL after freeing (inside free_generic)
 *     means a freed variable is safe to check with `if (ptr)` afterward,
 *     and guards against accidental double-free if the variable is
 *     reused before scope exit.
 *
 * USAGE:
 *
 *   int process(size_t n) {
 *       DEFER_MALLOC(int, nums, n);
 *       if (!nums) return -1;
 *       // ... use nums ...
 *       return 0; // freed automatically here, and on the line above
 *   }
 *
 *   int wrap(const char *s) {
 *       char *copy DEFER_FREE(copy) = strdup(s);
 *       if (!copy) return -1;
 *       // ... use copy ...
 *       return 0; // freed automatically here
 *   }
 *
 * Note: `cleanup` is a *variable attribute* -- it must appear on the
 * declaration line itself, the same way DEFER_MALLOC's attribute does.
 * You cannot declare a pointer first and attach cleanup afterward in a
 * separate statement; the attribute has to be part of the declarator.
 */

#ifndef DEFER_H
#define DEFER_H

#include <stdlib.h>

#if !defined(__GNUC__) && !defined(__clang__)
#  error "defer.h requires the GCC/Clang 'cleanup' variable attribute -- not supported on this compiler (e.g. MSVC)."
#endif

/*
 * free_generic -- generic cleanup callback for the `cleanup` attribute.
 *
 * Receives the *address* of the pointer variable it's attached to,
 * type-erased to void*. Reinterprets it as void** to reach the actual
 * pointer value, frees it if non-NULL, then nulls it out.
 *
 * Do not call this directly -- it's invoked automatically by the
 * compiler at scope exit for any variable annotated with
 * __attribute__((cleanup(free_generic))).
 */
static inline void free_generic(
  void *p
) {
  void **ptr = (void **)p;
  if (*ptr) {
    free(*ptr);
    *ptr = NULL;
  }
}

/* Internal: the raw attribute, named so call sites read intent clearly. */
#define DEFER_CLEANUP(func) __attribute__((cleanup(func)))

/*
 * DEFER_MALLOC(type, name, count)
 *
 * Declares `type *name`, allocates space for `count` elements of `type`
 * via malloc(), and arranges for it to be freed automatically at the
 * end of the enclosing scope.
 *
 * Equivalent to:
 *   type *name = malloc(count * sizeof(type));
 * plus an automatic free(name) inserted at every exit from this scope.
 *
 * `name` is NULL if allocation failed -- check it before use, as usual.
 */
#define DEFER_MALLOC(type, name, count) \
    type *name DEFER_CLEANUP(free_generic) = malloc((count) * sizeof(type))

/*
 * DEFER_FREE(name)
 *
 * Attaches automatic cleanup to a pointer variable being declared on
 * this line, without allocating anything. Use this for pointers you
 * received from elsewhere (strdup, a constructor function, etc.) that
 * you still own and are responsible for freeing.
 *
 * `name` is not used inside the macro expansion -- the `cleanup`
 * attribute only ever needs the variable it's attached to, which C
 * resolves implicitly from position in the declarator. The parameter
 * exists purely so the call site reads as self-documenting:
 *
 *   char *copy DEFER_FREE(copy) = strdup(input);
 *
 * Must appear on the same line as the declaration -- `cleanup` is a
 * variable attribute, not something addable after the fact:
 *
 *   char *copy = strdup(input);
 *   DEFER_FREE(copy); // WRONG -- too late, this is a separate statement
 */
#define DEFER_FREE(name) DEFER_CLEANUP(free_generic)

#endif /* DEFER_H */
