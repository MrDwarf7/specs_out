#ifndef H_PRELUDE_H
#define H_PRELUDE_H

// Look. Judge me all you want okay.
// I _really_ don't like C/C++'s native types.
// They're painful to write all the time, and
// I hate using auto anyway.
// This is the best middle ground. (and they're consistent across
// C and C++ for me so.
//

#include <stddef.h>
#include <stdint.h>

// char
typedef char          c8;           // 8-bit == char
typedef unsigned char uc8; // 8-bit == unsigned char

typedef size_t usize; // unsigned integer type for sizes

typedef uint8_t  u8;   // 8-bit == unsigned char
typedef uint16_t u16; // 16-bit == unsigned short /
typedef uint32_t u32; // 32-bit == unsigned int / unsigned long
typedef uint64_t u64; // 64-bit == unsigned long long

typedef int8_t  i8;   // 8-bit == signed char
typedef int16_t i16; // 16-bit == signed short
typedef int32_t i32; // 32-bit == signed int / signed long
typedef int64_t i64; // 64-bit == signed long long

typedef float  f32;  // floating-point 32-bit (single precision)
typedef double f64; // floating-point 64-bit (double precision)

typedef f32 float32; // floating-point 32-bit (single precision)
typedef f64 float64; // floating-point 64-bit (double precision)

typedef u8  byte;   // 8-bit   == same as unsigned char
typedef u16 word;  // 16-bit  == same as unsigned short
typedef u32 dword; // 32-bit  == same as unsigned int
typedef u64 qword; // 64-bit  == same as unsigned long long

#define KiB(n) ((u64)(n) << 10)
#define MiB(n) ((u64)(n) << 20)
#define GiB(n) ((u64)(n) << 30)
#define TiB(n) ((u64)(n) << 40)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define IS_EVEN_BITWISE(n) (((n) & 1) == 0)
#define IS_EVEN_REMAIN(n) (((n) % 2) == 0)

#define ALIGN_UP_POW2(n, p) (((u64)(n) + ((u64)(p) - 1)) & (~((u64)(p) - 1)))

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define PREFETCH(addr) __builtin_prefetch((addr), 0, 1)

#define ATTR_PACKED              __attribute__((packed))
#define ATTR_ALIGNED(n)          __attribute__((aligned(n)))
#define ATTR_PACKED_ALIGNED(n)   __attribute__((packed, aligned(n)))
#define ATTR_HOT                 __attribute__((hot))
#define ATTR_COLD                __attribute__((cold))
#define ATTR_PURE                __attribute__((pure))
#define ATTR_CONSTFN             __attribute__((const))
#define ATTR_NONNULL(...)        __attribute__((nonnull(__VA_ARGS__)))
#define ATTR_ALLOC_SIZE(...)     __attribute__((alloc_size(__VA_ARGS__)))
#define ATTR_RETURNS_NONNULL     __attribute__((returns_nonnull))
#define ATTR_ALWAYS_INLINE       __attribute__((always_inline)) static inline
#define ATTR_RESTRICT            restrict

#define ZERO_MEMORY(ptr, size) memset((ptr), 0, (size))
#define SET_MEMORY(ptr, value, size) memset((ptr), (value), (size))
#define COPY_MEMORY(dest, src, size) memcpy((dest), (src), (size))
#define COMPARE_MEMORY(ptr1, ptr2, size) memcmp((ptr1), (ptr2), (size))
#define MOVE_MEMORY(dest, src, size) memmove((dest), (src), (size))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ARRAY_INDEX_IN_BOUNDS(arr, index)                                      \
  ((index) >= 0 && (index) < (sizeof(arr) / sizeof((arr)[0])))

#define ARRAY_FILL(arr, size, value)                                           \
  do {                                                                         \
    for (size_t _i = 0; _i < (size); _i++) {                                   \
      (arr)[_i] = (value);                                                     \
    }                                                                          \
  } while (0)

#define SWAP_PTR(a, b)                                                         \
  do {                                                                         \
    void *tmp = (a);                                                           \
    (a) = (b);                                                                 \
    (b) = tmp;                                                                 \
  } while (0)

/*
 * ASCII upper/lower letters differ by bit 5 (32). Fold with OR, then the
 * letter test is (folded minus a) less than 26.
 *
 */
#define ASCII_CASE_BIT ((unsigned char)0x20)

static inline bool ascii_is_alpha(
  unsigned char code
) {
  return (unsigned char)((code | ASCII_CASE_BIT) - (unsigned char)'a') < 26U;
}

static inline unsigned char ascii_to_lower(
  unsigned char code
) {
  return ascii_is_alpha(code) ? (unsigned char)(code | ASCII_CASE_BIT) : code;
}

static inline unsigned char ascii_to_upper(
  unsigned char code
) {
  return ascii_is_alpha(code) ? (unsigned char)(code & (unsigned char)~ASCII_CASE_BIT) : code;
}

static inline bool ascii_eq_ci(
  const char *left, const char *right
) {
  if (!left || !right) return false;
  while (*left && *right) {
    unsigned char lhs = ascii_to_lower((unsigned char)*left++);
    unsigned char rhs = ascii_to_lower((unsigned char)*right++);
    if (lhs != rhs) return false;
  }
  return *left == *right;
}

#endif // H_PRELUDE_H
