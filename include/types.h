#ifndef TYPES_H
#define TYPES_H

#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* -------------------------------- compiler -------------------------------- */

#if defined(_MSC_VER)
#  define COMPILER_MSVC 1
#else
#  define COMPILER_MSVC 0
#endif

#if defined(__clang__)
#  define COMPILER_CLANG 1
#else
#  define COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !COMPILER_CLANG
#  define COMPILER_GCC 1
#else
#  define COMPILER_GCC 0
#endif

/* -------------------------- feature-test helpers -------------------------- */

#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif

#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

#ifndef __has_cpp_attribute
#  define __has_cpp_attribute(x) 0
#endif

#ifndef __has_include
#  define __has_include(x) 0
#endif

/* -------------------------------- alignof --------------------------------- */

#if COMPILER_MSVC
#  define ALIGNOF(T) __alignof(T)
#elif COMPILER_GCC || COMPILER_CLANG
#  define ALIGNOF(T) __alignof__(T)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define ALIGNOF(T) _Alignof(T)
#elif defined(__cplusplus)
#  define ALIGNOF(T) alignof(T)
#else
#  warning "alignof not supported"
#endif

/* ------------------------------ thread_local ------------------------------ */

#if COMPILER_MSVC
#  define THREAD_LOCAL __declspec(thread)
#elif COMPILER_GCC || COMPILER_CLANG
#  define THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define THREAD_LOCAL _Thread_local
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
#  define THREAD_LOCAL thread_local
#else
#  warning "thread_local not supported"
#endif

/* ------------------------------- endianness ------------------------------- */

#if (COMPILER_GCC || COMPILER_CLANG)
#  if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#    define ENDIAN_LITTLE (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#    define ENDIAN_BIG    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  else
#    define ENDIAN_LITTLE 0
#    define ENDIAN_BIG    0
#  endif
#elif COMPILER_MSVC
#  define ENDIAN_LITTLE 1
#  define ENDIAN_BIG    0
#else
#  define ENDIAN_LITTLE 0
#  define ENDIAN_BIG    0
#endif

/* ----------------------------- function name ------------------------------ */

/* short, portable name */
#if defined(__cplusplus)
#  define FUNC_NAME __func__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  define FUNC_NAME __func__
#elif COMPILER_MSVC
#  define FUNC_NAME __FUNCTION__
#else
#  define FUNC_NAME "(unknown)"
#endif

/* full signature when available */
#if (COMPILER_GCC || COMPILER_CLANG)
#  define FUNC_SIG __PRETTY_FUNCTION__
#elif COMPILER_MSVC
#  define FUNC_SIG __FUNCSIG__
#else
#  define FUNC_SIG FUNC_NAME
#endif

/* ------------------------------- attributes ------------------------------- */

#if __has_cpp_attribute(maybe_unused)
#  define MAYBE_UNUSED [[maybe_unused]]
#elif __has_attribute(unused) || (COMPILER_GCC || COMPILER_CLANG)
#  define MAYBE_UNUSED __attribute__((unused))
#elif COMPILER_MSVC
#  define MAYBE_UNUSED
#else
#  define MAYBE_UNUSED
#endif

#define UNUSED_VAR(x) \
  do {                \
    (void)(x);        \
  } while (0)

/* align specifier for variable declarations */
/* Usage: ALIGNED_VAR(uint8_t buf[1024], 64) = {0}; */
#if COMPILER_MSVC
#  define ALIGNED_VAR(DECL, N) __declspec(align(N)) DECL
#elif (COMPILER_GCC || COMPILER_CLANG)
#  define ALIGNED_VAR(DECL, N) DECL __attribute__((aligned(N)))
#else
#  define ALIGNED_VAR(DECL, N) DECL
#endif

/* align specifier for type declarations (struct/union/class) */
/* Usage:  ALIGNED_TYPE(struct, 32) Name { ...members... }; */
#if COMPILER_MSVC
#  define ALIGNED_TYPE(TYPE_KEYWORD, N) __declspec(align(N)) TYPE_KEYWORD
#elif COMPILER_GCC || COMPILER_CLANG
#  define ALIGNED_TYPE(TYPE_KEYWORD, N) TYPE_KEYWORD __attribute__((aligned(N)))
#else
#  define ALIGNED_TYPE(TYPE_KEYWORD, N) TYPE_KEYWORD
#endif

/* packed struct */
/* Usage:  PACKED_STRUCT(Name, { ...members... }); */
#if COMPILER_MSVC
#  define PACKED_STRUCT(NAME, BODY) __pragma(pack(push, 1)) struct NAME BODY __pragma(pack(pop))
#elif COMPILER_GCC || COMPILER_CLANG
#  define PACKED_STRUCT(NAME, BODY) struct __attribute__((packed)) NAME BODY
#else
#  define PACKED_STRUCT(NAME, BODY) struct NAME BODY
#endif

/* format(printf) for checking printf-like functions */
#if (COMPILER_GCC || COMPILER_CLANG) || __has_attribute(format)
/* 1-based indices: fmt starts at 'fmt_index', args at 'vararg_index' */
#  define ATTR_FORMAT(fmt_index, vararg_index) __attribute__((format(printf, fmt_index, vararg_index)))
#else
#  define ATTR_FORMAT(fmt_index, vararg_index)
#endif

/* -------------------------- debugbreak / assert --------------------------- */

#if COMPILER_MSVC
#  define DEBUGBREAK() __debugbreak()
#elif __has_builtin(__builtin_debugtrap)
#  define DEBUGBREAK() __builtin_debugtrap()
#elif __has_builtin(__builtin_trap)
#  define DEBUGBREAK() __builtin_trap()
#else
#  include <signal.h>
#  define DEBUGBREAK() raise(SIGTRAP)
#endif

#if defined BUILD_DEBUG || defined BUILD_TEST_UI
void
my_assert_msg(const char *expr, const char *file, int line, const char *fmt, ...) ATTR_FORMAT(4, 5);
void
my_assert(const char *expr, const char *file, int line);

#  define ASSERT(COND)                        \
    do {                                      \
      if (!(COND)) {                          \
        my_assert(#COND, __FILE__, __LINE__); \
      }                                       \
    } while (0)
#  define MASSERT(COND, ...)                                   \
    do {                                                       \
      if (!(COND)) {                                           \
        my_assert_msg(#COND, __FILE__, __LINE__, __VA_ARGS__); \
      }                                                        \
    } while (0)
#else
#  define ASSERT(COND) \
    do {               \
      (void)(COND);    \
    } while (0)
#  define MASSERT(COND, ...) \
    do {                     \
      (void)(COND);          \
    } while (0)
#endif

/* static assert */
#if defined(__cplusplus) || (COMPILER_MSVC && (_MSC_VER >= 1600))
#  define STATIC_ASSERT(COND, MSG) static_assert((COND), MSG)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define STATIC_ASSERT(COND, MSG) _Static_assert((COND), MSG)
#endif

/* ---------------------------------- misc ---------------------------------- */

#ifndef ON
#  define ON 1
#endif

#ifndef OFF
#  define OFF 0
#endif

#define AS_BOOL(x) !!(x)
#define FLAG(bit)  (1 << (bit))

#define IN_RANGE(v, min, max) (((v) >= (min)) && ((v) <= (max)))

#define MAX_VAL(a, b) (((a) > (b)) ? (a) : (b))
#define MIN_VAL(a, b) (((a) < (b)) ? (a) : (b))

#define CLAMP(v, min, max)   (((v) > (max)) ? (max) : (((v) < (min)) ? (min) : (v)))
#define CLAMP_TOP(v, max)    MIN_VAL(v, max)
#define CLAMP_BOTTOM(v, min) MAX_VAL(v, min)

#ifndef COUNTOF
#  define COUNTOF(array) (int)(sizeof(array) / sizeof((array)[0]))
#endif

#define BOOL_AS_STR(v) ((v) ? "true" : "false")

// alignment must be a power-of-two
#define ALIGN_DOWN(size, alignment) ((size) & ~((alignment) - 1))
#define ALIGN_UP(size, alignment)   ALIGN_DOWN(size + alignment - 1, alignment)

#define IS_POW2(v) (((v) & ((v) - 1)) == 0)

#define CAT2(a, b) a##b
#define CAT(a, b)  CAT2(a, b)

#define UNIQUE_NAME(prefix) CAT(prefix, __LINE__)

#define ENSURE_STR_LIT(x) "" x ""

#define KB (1024 * 1ULL)
#define MB (1024 * KB)
#define GB (1024 * MB)
#define TB (1024 * GB)

#define QUEUE_PUSH(f, l, n) \
  ((f) == 0 ? (((f) = (l) = (n)), ((n)->next = 0)) : ((l)->next = (n), (l) = (n), ((n)->next = 0)))
#define QUEUE_PUSH_FRONT(f, l, n) \
  ((f) == 0 ? (((f) = (l) = (n)), ((n)->next = 0)) : ((n)->next = (f)), ((f) = (n)))
#define QUEUE_POP(f, l) \
  ((f) == (l) ? ((f) = 0, (l) = 0) : ((f) = (f)->next))

#define STACK_PUSH(f, n) \
  ((n)->next = (f), (f) = (n))
#define STACK_POP(f) \
  ((f) == 0 ? 0 : ((f) = (f)->next))

/* -------------------------------- helpers --------------------------------- */
static inline void *
mem_zero(void *ptr, uint64_t size)
{
#if __has_builtin(__builtin_memset) || COMPILER_GCC || COMPILER_CLANG
  __builtin_memset(ptr, 0, size);
#else
  uint8_t *buf = (uint8_t *)ptr;
  for (uint64_t i = 0; i < size; ++i) {
    buf[i] = 0;
  }
#endif
  return ptr;
}

static inline void *
mem_set(void *ptr, uint8_t val, uint64_t size)
{
#if __has_builtin(__builtin_memset) || COMPILER_GCC || COMPILER_CLANG
  __builtin_memset(ptr, val, size);
#else
  uint8_t *buf = (uint8_t *)ptr;
  for (uint64_t i = 0; i < size; ++i) {
    buf[i] = val;
  }
#endif
  return ptr;
}

static inline void *
mem_copy(void *dst, void *src, uint64_t size)
{
#if __has_builtin(__builtin_memcpy) || COMPILER_GCC || COMPILER_CLANG
  __builtin_memcpy(dst, src, size);
#else
  uint8_t *buf_dst = (uint8_t *)dst;
  uint8_t *buf_src = (uint8_t *)src;
  for (uint64_t i = 0; i < size; ++i) {
    buf_dst[i] = buf_src[i];
  }
#endif
  return dst;
}

static inline void *
mem_move(void *dst, void *src, uint64_t size)
{
  uint8_t *d = (uint8_t *)dst;
  uint8_t *s = (uint8_t *)src;

  if (d == s || size == 0) {
    return dst;
  }

  if (d < s || d >= s + size) {
    /* no overlap or dst is before src - copy forward */
    for (uint64_t i = 0; i < size; ++i) {
      d[i] = s[i];
    }
  } else {
    /* dst overlaps src from behind - copy backward */
    for (uint64_t i = size; i > 0; --i) {
      d[i - 1] = s[i - 1];
    }
  }

  return dst;
}

uint64_t
time_now_us(void);

static inline uint64_t
time_snap_up(uint64_t t, uint32_t q)
{
  return (q == 0) ? (t) : (t + q - 1) / (q * q);
}

static inline uint64_t
time_us_to_ms(uint64_t us)
{
  return (us + 999) / 1000;
}

bool
float_equal(float a, float b, float min_val, float step);

#include "config.h"

#endif /* TYPES_H */
