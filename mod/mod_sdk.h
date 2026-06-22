#ifndef MOD_SDK_H
#define MOD_SDK_H

#include "mod_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================== TYPES ===================================================== */

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

#if defined(_MSC_VER)
#  define MOD_ALIGNOF(T) __alignof(T)
#elif defined(__clang__) || defined(__GNUC__)
#  define MOD_ALIGNOF(T) __alignof__(T)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define MOD_ALIGNOF(T) _Alignof(T)
#elif defined(__cplusplus)
#  define MOD_ALIGNOF(T) alignof(T)
#else
#  warning "alignof not supported"
#endif

/* align specifier for type declarations (struct/union/class) */
/* Usage:  ALIGNED_TYPE(struct, 32) Name { ...members... }; */
#if defined(_MSC_VER)
#  define MOD_ALIGNED_TYPE(TYPE_KEYWORD, N) __declspec(align(N)) TYPE_KEYWORD
#elif defined(__clang__) || defined(__GNUC__)
#  define MOD_ALIGNED_TYPE(TYPE_KEYWORD, N) TYPE_KEYWORD __attribute__((aligned(N)))
#else
#  define MOD_ALIGNED_TYPE(TYPE_KEYWORD, N) TYPE_KEYWORD
#endif

/* format(printf) for checking printf-like functions */
#if defined(__clang__) || defined(__GNUC__) || __has_attribute(format)
/* 1-based indices: fmt starts at 'fmt_index', args at 'vararg_index' */
#  define MOD_ATTR_FORMAT(fmt_index, vararg_index) __attribute__((format(printf, fmt_index, vararg_index)))
#else
#  define MOD_ATTR_FORMAT(fmt_index, vararg_index)
#endif

#if !defined MOD_DISABLE_ASSERT
/*
 * Shows an assertion dialog with the failed expression and optional message.
 * Abort exits the process, Retry breaks into the debugger, and Ignore continues.
 */
void
mod_assert_msg(const char *expr, const char *file, int line, const char *fmt, ...) MOD_ATTR_FORMAT(4, 5);

#  define MOD_ASSERT(COND)                               \
    do {                                                 \
      if (!(COND)) {                                     \
        mod_assert_msg(#COND, __FILE__, __LINE__, NULL); \
      }                                                  \
    } while (0)
#  define MOD_ASSERT_MSG(COND, ...)                             \
    do {                                                        \
      if (!(COND)) {                                            \
        mod_assert_msg(#COND, __FILE__, __LINE__, __VA_ARGS__); \
      }                                                         \
    } while (0)
#else
#  define MOD_ASSERT(COND) \
    do {                   \
      (void)sizeof(COND);  \
    } while (0)
#  define MOD_ASSERT_MSG(COND, ...) \
    do {                            \
      (void)sizeof(COND);           \
    } while (0)
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

/* alignment must be a power-of-two */
#define ALIGN_DOWN(size, alignment) ((size) & ~((alignment) - 1))
#define ALIGN_UP(size, alignment)   ALIGN_DOWN(size + alignment - 1, alignment)

#define ENSURE_STR_LIT(x) "" x ""
#define BOOL_AS_STR(v) ((v) ? "true" : "false")

#define KB (1024 * 1ULL)
#define MB (1024 * KB)
#define GB (1024 * MB)
#define TB (1024 * GB)

/* ======================================================= SDK  ===================================================== */

typedef struct {
  const mod_host_api_t *host;
  mod_t                 mod;
} mod_sdk_t;

/* Returns the host API saved by mod_sdk_init. Asserts if the SDK is not initialized. */
const mod_host_api_t *
mod_sdk_host(void);
/* Returns this mod's loader handle. Asserts if the SDK is not initialized. */
mod_t
mod_sdk_mod_handle(void);

/*
 * Checks the host API size and ABI version, then stores the host API and mod handle.
 * This must succeed before any other SDK function is used. Major and minor ABI versions must match.
 */
bool
mod_sdk_init(const mod_host_api_t *host, mod_t mod);

extern mod_sdk_t g_sdk;

/* ======================================================= MOD  ===================================================== */

#define MOD_LOG_ERROR(...) mod_log(LOG_LEVEL_ERROR, __VA_ARGS__)
#define MOD_LOG_WARN(...)  mod_log(LOG_LEVEL_WARN, __VA_ARGS__)
#define MOD_LOG_INFO(...)  mod_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define MOD_LOG_DEBUG(...) mod_log(LOG_LEVEL_DEBUG, __VA_ARGS__)

/* Writes a formatted message to the loader log. */
void
mod_log(log_level_t level, const char *fmt, ...);
/* Writes a formatted message to the loader log using an existing va_list. */
void
mod_logv(log_level_t level, const char *fmt, va_list args);

/* Returns a loader-owned string of this mod's directory path. */
str_t
mod_get_mod_dir(void);
/* Returns a loader-owned string of this mod's config file path. */
str_t
mod_get_config_path(void);
/* Returns a loader-owned string of this mod's manifest file path. */
str_t
mod_get_manifest_path(void);
/* Finds a config entry declared by this mod. Returns MOD_CFG_INVALID when it is not found. */
mod_cfg_t
mod_get_cfg_by_id(str_t id);
/* Registers a console command callback for this mod. Returns false if registration fails. */
bool
mod_register_cmd(str_t name, str_t description, mod_cmd_fn_t fn, void *user);

/* ======================================================== UI ====================================================== */

/* Returns the loader's Nuklear context. Use it to draw UI (in tick, draw_panel, draw_config). */
struct nk_context *
mod_get_nk_ctx(void);
/* Writes the current viewport size. Either output pointer may be NULL. */
void
mod_get_viewport_size(unsigned int *vw, unsigned int *vh);

/* ======================================================= CFG  ===================================================== */

/* Returns a bool config value, or false when the handle cannot provide one. */
bool
mod_cfg_get_bool(mod_cfg_t cfg);
/* Returns an integer config value, or 0 when the handle cannot provide one. */
int
mod_cfg_get_int(mod_cfg_t cfg);
/* Returns a float config value, or 0.0f when the handle cannot provide one. */
float
mod_cfg_get_float(mod_cfg_t cfg);
/* Returns an enum config index, or -1 when the handle cannot provide one. */
int
mod_cfg_get_enum(mod_cfg_t cfg);
/* Returns the string config length in bytes, not including a null terminator. */
uint64_t
mod_cfg_get_string_len(mod_cfg_t cfg);
/* Copies up to cap bytes from a string config and returns the number copied. No null terminator is added. */
uint64_t
mod_cfg_get_string_data(mod_cfg_t cfg, void  *buf, uint64_t cap);
/* Copies a string config into an arena and adds a null terminator. Returns STR_NULL for an empty or invalid value. */
str_t
mod_cfg_push_string(mod_cfg_t cfg, mod_arena_t arena);
/* Returns a keybind config value, or an empty keybind when the handle cannot provide one. */
keybind_t
mod_cfg_get_keybind(mod_cfg_t cfg);
/* Returns a color config value, or zeroed color data when the handle cannot provide one. */
mod_color_t
mod_cfg_get_color(mod_cfg_t cfg);
/* Sets a bool config value when the handle refers to a valid and compatible entry. */
void
mod_cfg_set_bool(mod_cfg_t cfg, bool val);
/* Sets an integer config value when the handle refers to a valid and compatible entry. */
void
mod_cfg_set_int(mod_cfg_t cfg, int val);
/* Sets a float config value when the handle refers to a valid and compatible entry. */
void
mod_cfg_set_float(mod_cfg_t cfg, float val);
/* Sets an enum config index when the handle refers to a valid and compatible entry. */
void
mod_cfg_set_enum(mod_cfg_t cfg, int val);
/* Sets a string config value. The loader copies the supplied bytes. */
void
mod_cfg_set_string(mod_cfg_t cfg, str_t val);
/* Sets a keybind config value when the handle refers to a valid and compatible entry. */
void
mod_cfg_set_keybind(mod_cfg_t cfg, keybind_t val);
/* Sets a color config value when the handle refers to a valid and compatible entry. */
void
mod_cfg_set_color(mod_cfg_t cfg, mod_color_t val);

/* ====================================================== ARENA ===================================================== */

#define MOD_ARENA_PUSH_ARRAY(ARENA, T, N)      (T *)mod_arena_push((ARENA), (N) * sizeof(T), MOD_ALIGNOF(T))
#define MOD_ARENA_PUSH_ARRAY_ZERO(ARENA, T, N) (T *)mod_arena_push_zero((ARENA), (N) * sizeof(T), MOD_ALIGNOF(T))
#define MOD_ARENA_PUSH(ARENA, T)               MOD_ARENA_PUSH_ARRAY(ARENA, T, 1)
#define MOD_ARENA_PUSH_ZERO(ARENA, T)          MOD_ARENA_PUSH_ARRAY_ZERO(ARENA, T, 1)

/*
 * Returns this mod's permanent arena.
 * Allocations stay valid until the mod is unloaded.
 * Destroyed automatically by the loader after mod is deinitialized (i.e. deinit called).
 */
mod_arena_t
mod_get_perm(void);
/*
 * Creates a dynamic arena owned by this mod. Returns MOD_ARENA_INVALID on failure.
 * Destroyed automatically by the loader after mod is deinitialized (i.e. deinit called).
 */
mod_arena_t
mod_arena_create(uint64_t reserve_size, uint64_t commit_size);
/*
 * Destroys an arena created by mod_arena_create.
 * Only arenas created by this mod can be destroyed.
 * Permanent arena and scratch arena cannot be destroyed.
 */
void
mod_arena_destroy(mod_arena_t arena);
/* Allocates bytes from an arena with the requested alignment. Returns NULL when the allocation cannot fit. */
void *
mod_arena_push(mod_arena_t arena, uint64_t size, uint64_t alignment);
/* Allocates aligned bytes from an arena and clears the whole allocation. */
void *
mod_arena_push_zero(mod_arena_t arena, uint64_t size, uint64_t alignment);
/* Returns the current used byte position of an arena. */
uint64_t
mod_arena_pos(mod_arena_t arena);
/*
 * Rewinds an arena to zero.
 * Memory is NOT decommitted.
 * Use this to quickly clear the arena instead of mod_arena_pop/mod_arena_pop_to (expensive).
 */
void
mod_arena_reset(mod_arena_t arena);
/*
 * Rewinds an arena to a previous byte position.
 * Later allocations become invalid. Memory is decommitted.
 */
void
mod_arena_pop_to(mod_arena_t arena, uint64_t pos);
/*
 * Rewinds an arena by up to size bytes.
 * Popping past the start resets it to zero.
 * Later allocations become invalid. Memory is decommitted.
 */
void
mod_arena_pop(mod_arena_t arena, uint64_t size);
/*
 * Returns a temporary (scratch) arena that is different from the supplied conflicting arena.
 * Pass MOD_ARENA_INVALID when there is no conflict. Pair every call with mod_scratch_end.
 */
tmp_arena_t
mod_scratch_begin(mod_arena_t conflict);
/* Ends a scratch arena and invalidates allocations made after that scope began. */
void
mod_scratch_end(tmp_arena_t tmp);

/* ====================================================== INPUT ===================================================== */

/* Converts a key name to an input key value. Returns INPUT_KEY_NONE when unknown. */
input_key_kind_t
mod_input_key_from_str(str_t name);
/* Returns a loader-owned name for an input key, or STR_NULL when unknown. */
str_t
mod_input_key_to_str(input_key_kind_t key);
/* Returns a loader-owned name for an input event kind, or STR_NULL when unknown. */
str_t
mod_input_event_kind_to_str(input_event_kind_t kind);
/* Parses a keybind string into out. Returns false for an empty or invalid bind. */
bool
mod_keybind_parse(str_t str, keybind_t *out);
/* Returns the formatted keybind length in bytes, not including a null terminator. */
uint64_t
mod_keybind_utf8_len(keybind_t bind);
/* Writes a formatted keybind into a buffer and returns the bytes written. No null terminator is added. */
uint64_t
mod_keybind_utf8_write(keybind_t bind, void *buf, uint64_t cap);
/* Formats a keybind into an arena and adds a null terminator. */
str_t
mod_keybind_to_str(keybind_t bind, mod_arena_t arena);
/* Returns whether the keybind is down in the current input state. */
bool
mod_keybind_is_down(keybind_t bind);
/* Returns whether the keybind was down in the previous input state. */
bool
mod_keybind_was_down(keybind_t bind);
/* Returns true on the frame a keybind changes from up to down. */
bool
mod_keybind_is_pressed(keybind_t bind);
/* Returns true on the frame a keybind changes from down to up. */
bool
mod_keybind_is_released(keybind_t bind);
/* Parses a keybind string and checks its current state. Invalid strings return false. */
bool
mod_keybind_str_is_down(str_t keybind_str);
/* Parses a keybind string and checks its previous state. Invalid strings return false. */
bool
mod_keybind_str_was_down(str_t keybind_str);
/* Parses a keybind string and checks for an up to down transition. */
bool
mod_keybind_str_is_pressed(str_t keybind_str);
/* Parses a keybind string and checks for a down to up transition. */
bool
mod_keybind_str_is_released(str_t keybind_str);
/* Returns whether the event key appears in the bind. It does not check event kind or other key states. */
bool
mod_input_event_covers_keybind(input_event_t *ev, keybind_t bind);

/* ========================================== HOOKING / SIGNATURE SCANNING ========================================== */

typedef struct sigscan_entry_s sigscan_entry_t;
struct sigscan_entry_s {
  const char *name;          // for debugging/logging
  const char *pattern;       // signature pattern, e.g. "40 55 56 ?? ..."
  void      **pp;            // write back pointer, e.g. &ProcessEvent
  uint8_t     op_off;        // offset of the operand inside the matched bytes
  uint8_t     deref;         // 0..2 times *(ptr)
  void       *addr;          // resulting address
  uintptr_t   first_rvas[8]; // first RVAs to try
};

/* using macros for templates because they're constant */

#define SIG_ENTRY_DIRECT(PTR, PATTERN, ...) \
  {                                         \
    .name       = #PTR,                     \
    .pattern    = (PATTERN),                \
    .pp         = (void **)&PTR,            \
    .op_off     = 0,                        \
    .deref      = 0,                        \
    .addr       = NULL,                     \
    .first_rvas = {__VA_ARGS__},            \
  }

#define SIG_ENTRY_MOV64_PTR(PTR, PATTERN, ...) \
  {                                            \
    .name       = #PTR,                        \
    .pattern    = (PATTERN),                   \
    .pp         = (void **)&PTR,               \
    .op_off     = 3,                           \
    .deref      = 1,                           \
    .addr       = NULL,                        \
    .first_rvas = {__VA_ARGS__},               \
  }

#define SIG_ENTRY_MOV64_ADDR(PTR, PATTERN, ...) \
  {                                             \
    .name       = #PTR,                         \
    .pattern    = (PATTERN),                    \
    .pp         = (void **)&PTR,                \
    .op_off     = 3,                            \
    .deref      = 0,                            \
    .addr       = NULL,                         \
    .first_rvas = {__VA_ARGS__},                \
  }

#define SIG_ENTRY_MOV32_PTR(PTR, PATTERN, ...) \
  {                                            \
    .name       = #PTR,                        \
    .pattern    = (PATTERN),                   \
    .pp         = (void **)&PTR,               \
    .op_off     = 2,                           \
    .deref      = 1,                           \
    .addr       = NULL,                        \
    .first_rvas = {__VA_ARGS__},               \
  }

#define SIG_ENTRY_LEA_ADDR(PTR, PATTERN, ...) \
  {                                           \
    .name       = #PTR,                       \
    .pattern    = (PATTERN),                  \
    .pp         = (void **)&PTR,              \
    .op_off     = 3,                          \
    .deref      = 0,                          \
    .addr       = NULL,                       \
    .first_rvas = {__VA_ARGS__},              \
  }

#define SIG_ENTRY_CALL_ARG(PTR, PATTERN, ...) \
  {                                           \
    .name       = #PTR,                       \
    .pattern    = (PATTERN),                  \
    .pp         = (void **)&PTR,              \
    .op_off     = 1,                          \
    .deref      = 0,                          \
    .addr       = NULL,                       \
    .first_rvas = {__VA_ARGS__},              \
  }

/*
 * Tries the listed RVAs first, then scans the configured game module ranges for the pattern.
 * op_off selects a RIP-relative operand when nonzero, and deref controls how many pointer reads follow.
 * The resolved address is written through entry->pp and a SIGSCAN_ERR value is returned.
 */
sigscan_err_t
mod_sigscan(sigscan_entry_t *entry);
/*
 * Creates a hook owned by this mod and writes the trampoline to original.
 * The hook starts disabled.
 * Removed automatically by the loader after mod is deinitialized (i.e. deinit called).
 */
bool
mod_hook_create(void *target, void *detour, void **original);
/* Enables a hook previously created for target. */
bool
mod_hook_enable(void *target);
/* Disables a hook previously created for target. */
bool
mod_hook_disable(void *target);
/* Removes a hook previously created for target and releases its loader state. */
bool
mod_hook_remove(void *target);

/* ===================================================== STRING ===================================================== */

#define STR_CLIT(lit) {.data = (uint8_t *)(ENSURE_STR_LIT(lit)), .len = (sizeof(lit) - 1)}

#ifdef __cplusplus
#  define STR_LIT(lit) str_t STR_CLIT(lit)
#  define STR_NULL     str_t{}
#else
#  define STR_LIT(lit) (str_t)STR_CLIT(lit)
#  define STR_NULL     (str_t){0}
#endif

#define STR_FMT      "%.*s"
#define STR_ARG(str) (int)(str).len, (str).data

#define STR_BOOL(v)  ((v) ? STR_LIT("true") : STR_LIT("false"))

typedef uint32_t str_cmp_flags_t;
enum {
  STR_CMP_FLAG_IGNORE_CASE    = (1 << 0),
  STR_CMP_FLAG_IGNORE_SLASHES = (1 << 1),
};

typedef struct str_node_s str_node_t;
struct str_node_s {
  struct str_node_s *next;
  str_t              str;
};

typedef struct str_list_s str_list_t;
struct str_list_s {
  str_node_t *first;
  str_node_t *last;
  uint64_t    count;
  uint64_t    total_bytes;
};

typedef struct str_array_s str_array_t;
struct str_array_s {
  uint64_t count;
  str_t   *items;
};

static inline bool
str_is_empty(str_t s)
{
  return (!s.data || s.len == 0);
}

static inline bool
str16_is_empty(str16_t s)
{
  return (!s.data || s.len == 0);
}

static inline uint8_t
ascii_is_space(uint8_t c)
{
  return c == ' ' || c == '\r' || c == '\t' || c == '\f' || c == '\v' || c == '\n';
}

static inline uint8_t
ascii_to_upper(uint8_t c)
{
  return (c >= 'a' && c <= 'z') ? ('A' + (c - 'a')) : c;
}

static inline uint8_t
ascii_to_lower(uint8_t c)
{
  return (c >= 'A' && c <= 'Z') ? ('a' + (c - 'A')) : c;
}

static inline uint8_t
ascii_is_num(uint8_t c)
{
  return c >= '0' && c <= '9';
}

static inline uint8_t
ascii_is_alpha_lower(uint8_t c)
{
  return c >= 'a' && c <= 'z';
}

static inline uint8_t
ascii_is_alpha_upper(uint8_t c)
{
  return c >= 'A' && c <= 'Z';
}

static inline uint8_t
ascii_is_alpha(uint8_t c)
{
  return ascii_is_alpha_lower(c) || ascii_is_alpha_upper(c);
}

static inline uint8_t
ascii_to_forward_slash(uint8_t c)
{
  return (c == '\\' ? '/' : c);
}

/* Lowercases an ASCII letter stored in a UTF-16 code unit. */
static inline uint16_t
utf16_ascii_to_lower(uint16_t c)
{
  return (c >= 'A' && c <= 'Z') ? (uint16_t)('a' + (c - 'A')) : c;
}

/* Converts a value from 0 to 15 to an uppercase hexadecimal byte. */
static inline uint8_t
nibble_to_hex(uint8_t d)
{
  char c = 0;

  if (d < 10) {
    c = d + '0';
  } else {
    c = d - 10 + 'A';
  }

  return c;
}

/* Converts a hexadecimal byte to a value from 0 to 15. Returns -1 when invalid. */
static inline int
hex_to_nibble(uint8_t c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

/* Compares two ASCII bytes using the requested case and slash rules. */
static inline bool
ascii_equal(uint8_t a, uint8_t b, str_cmp_flags_t flags)
{
  if (flags & STR_CMP_FLAG_IGNORE_SLASHES) {
    a = ascii_to_forward_slash(a);
    b = ascii_to_forward_slash(b);
  }

  if (flags & STR_CMP_FLAG_IGNORE_CASE) {
    a = ascii_to_lower(a);
    b = ascii_to_lower(b);
  }

  return a == b;
}

/* Length calculation */

/* Returns the byte length of a null-terminated string. NULL returns zero. */
uint64_t
calc_cstr_len(const char *cstr);
/* Returns the byte length up to cap bytes, even when no null terminator is found. */
uint64_t
calc_cstr_len_with_cap(const char *s, uint64_t cap);
/* Returns the UTF-16 code unit length of a null-terminated wide string. NULL returns zero. */
uint64_t
calc_wstr_len(const wchar_t *wstr);
/* Returns the wide string length up to cap code units, even when no terminator is found. */
uint64_t
calc_wstr_len_with_cap(const wchar_t *s, uint64_t cap);

/* Creation */

/* Creates a string view. Invalid data or zero size produces STR_NULL. */
str_t
str_make(void *data, uint64_t size);
/* Creates a string view of a null-terminated byte string, excluding the terminator. */
str_t
str_from_cstr(const char *cstr);
/* Creates a string view limited to cap bytes. */
str_t
str_from_cstr_with_cap(const char *cstr, uint64_t cap);
/* Creates a UTF-16 string view. Invalid data or zero size produces an empty view. */
str16_t
str16_make(uint16_t *data, uint64_t size);
/* Creates a UTF-16 string view of a null-terminated wide string. */
str16_t
str16_from_wstr(const wchar_t *wstr);
/* Creates a UTF-16 string view limited to cap code units. */
str16_t
str16_from_wstr_with_cap(const wchar_t *wstr, uint64_t cap);
/* Returns a clamped string slice. Reversed indexes are swapped. */
str_t
str_slice(str_t str, uint64_t start_idx, uint64_t end_idx);

/* Comparison/Search */

/* Checks whether a string starts with a prefix using the requested comparison flags. */
bool
str_has_prefix(str_t s, str_t prefix, str_cmp_flags_t flags);
/* Checks whether a string ends with a suffix using the requested comparison flags. */
bool
str_has_suffix(str_t s, str_t suffix, str_cmp_flags_t flags);
/* Compares two strings byte by byte and returns a value below, equal to, or above zero. */
int
str_compare(str_t a, str_t b, str_cmp_flags_t flags);
/* Checks whether two strings contain the same bytes under the requested comparison flags. */
bool
str_equal(str_t a, str_t b, str_cmp_flags_t flags);
/* Checks whether two strings are equal ignoring the case. */
bool
str_equal_icase(str_t a, str_t b);
/* Finds the first matching substring and optionally writes its byte index. */
bool
str_find(str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx);
/* Finds the last matching substring and optionally writes its byte index. */
bool
str_rfind(str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx);
/* Finds the first byte present in charset and optionally writes its index. */
bool
str_find_any(str_t s, str_t charset, str_cmp_flags_t flags, uint64_t *idx);
/* Finds the last byte present in charset and optionally writes its index. */
bool
str_rfind_any(str_t s, str_t charset, str_cmp_flags_t flags, uint64_t *idx);
/* Checks whether a string contains a byte under the requested comparison flags. */
bool
str_has_char(str_t s, char c, str_cmp_flags_t flags);

/* Allocation */

/* Copies a string into an arena and adds a null terminator. */
str_t
str_push_copy(mod_arena_t arena, str_t str);
/* Formats into a buffer and returns the required byte count excluding the terminator. The result may be truncated. */
uint64_t
str_write_vfmt(void *buf, uint64_t cap, const char *fmt, va_list args);
/* Formats into a buffer and returns the required byte count excluding the terminator. The result may be truncated. */
uint64_t
str_write_fmt(void *buf, uint64_t cap, const char *fmt, ...) MOD_ATTR_FORMAT(3, 4);
/* Formats a null-terminated string into an arena using an existing va_list. */
str_t
str_push_vfmt(mod_arena_t arena, const char *fmt, va_list args);
/* Formats a null-terminated string into an arena. */
str_t
str_push_fmt(mod_arena_t arena, const char *fmt, ...) MOD_ATTR_FORMAT(2, 3);
/* Allocates a string filled with one byte and adds a null terminator. */
str_t
str_push_fill_byte(mod_arena_t arena, uint64_t size, uint8_t byte);
/* Copies two strings into one arena allocation and adds a null terminator. */
str_t
str_push_concat(mod_arena_t arena, str_t a, str_t b);
/* Formats raw bytes as uppercase hexadecimal and adds a null terminator. */
str_t
str_push_hex(mod_arena_t arena, void *data, uint64_t size);

/* String list/array */

/* Deep-copies a string list, including every string, into an arena. */
str_list_t
str_list_copy(mod_arena_t arena, str_list_t src);
/* Appends an existing node and updates list totals. The node's next pointer is overwritten. */
void
str_list_push_node(str_list_t *list, str_node_t *n);
/* Prepends an existing node and updates list totals. The node's next pointer is overwritten. */
void
str_list_push_node_front(str_list_t *list, str_node_t *n);
/* Allocates and appends a node that keeps a view of str. */
void
str_list_push(mod_arena_t arena, str_list_t *list, str_t str);
/* Copies a string into an arena, then appends it as a new list node. */
void
str_list_push_copy(mod_arena_t arena, str_list_t *list, str_t str);
/* Formats a string into an arena and appends it as a new list node. */
void
str_list_push_fmt(mod_arena_t arena, str_list_t *list, const char *fmt, ...) MOD_ATTR_FORMAT(3, 4);
/* Allocates and prepends a node that keeps a view of str. */
void
str_list_push_front(mod_arena_t arena, str_list_t *list, str_t str);
/* Formats a string into an arena and prepends it as a new list node. */
void
str_list_push_front_fmt(mod_arena_t arena, str_list_t *list, const char *fmt, ...) MOD_ATTR_FORMAT(3, 4);
/* Checks whether any list item equals a string under the requested comparison flags. */
bool
str_list_contains(str_list_t list, str_t s, str_cmp_flags_t flags);
/* Moves all nodes from src to the end of dst, then clears src. */
void
str_list_concat_inplace(str_list_t *dst, str_list_t *src);
/* Splits a string on any supplied separator. Nodes are allocated, but their strings remain views of the input. */
str_list_t
str_split(mod_arena_t arena, str_t str, uint64_t split_count, str_t *splits);
/* Splits text on CR or LF and trims trailing whitespace. Returned strings remain views of the input. */
str_list_t
str_split_lines(mod_arena_t arena, str_t str);
/* Joins a list with optional prefix, separator, and suffix into one null-terminated arena string. */
str_t
str_list_join(mod_arena_t arena, str_list_t list, str_t pre, str_t sep, str_t post);
/* Joins a list with LF separators into one null-terminated arena string. */
str_t
str_list_join_lines(mod_arena_t arena, str_list_t list);
/* Creates a string array view over existing string items. */
str_array_t
str_array_make(str_t *items, int count);
/* Deep-copies a string array and all of its strings into an arena. */
str_array_t
str_array_copy(mod_arena_t arena, str_array_t a);
/* Deep-copies a list into a flat string array. */
str_array_t
str_array_copy_from_list(mod_arena_t arena, str_list_t list);
/* Copies only the item views from a list into a flat array. String data is not copied. */
str_array_t
str_array_from_list(mod_arena_t arena, str_list_t list);
/* Checks whether an array contains a string under the requested comparison flags. */
bool
str_array_contains(str_array_t array, str_t s, str_cmp_flags_t flags);
/* Finds a matching array item and optionally writes its index. */
bool
str_array_find(str_array_t array, str_t s, str_cmp_flags_t flags, uint64_t *idx);

/* UTF-8 <-> UTF-16 conversions */

/* Converts UTF-16 to a null-terminated UTF-8 arena string. Invalid input units become question marks. */
str_t
str_from_str16(mod_arena_t arena, str16_t in);
/* Converts UTF-8 to a null-terminated UTF-16 arena string. Invalid input bytes become question marks. */
str16_t
str16_from_str(mod_arena_t arena, str_t in);

/* Returns the UTF-8 byte count needed for a UTF-16 string, excluding a terminator. */
uint64_t
str16_utf8_len(str16_t in);
/* Writes complete UTF-8 codepoints that fit and returns the byte count. No terminator is added. */
uint64_t
str16_write_utf8(uint8_t *buf, uint64_t max_len, str16_t in);

/* C-string conversion */

/* Copies a byte string into an arena and returns a null-terminated C string. */
char *
str_push_cstr(mod_arena_t arena, str_t s);
/* Copies a UTF-16 string into an arena and returns a null-terminated wide string. */
wchar_t *
str16_push_wstr(mod_arena_t arena, str16_t s);

/* Copies a string with a null terminator. The return value includes the terminator byte. */
uint64_t
str_write_cstr(str_t s, char *buf, uint64_t cap);
/* Copies up to cap raw string bytes and returns the number copied. No terminator is added. */
uint64_t
str_write_data(str_t s, char *buf, uint64_t cap);

/* Mutations */

/* Copies a string into an arena and uppercases ASCII letters. */
str_t
str_to_upper(mod_arena_t arena, str_t str);
/* Copies a string into an arena and lowercases ASCII letters. */
str_t
str_to_lower(mod_arena_t arena, str_t str);

/* Trimming */

/* Returns a view with leading bytes from charset removed. */
str_t
str_trim_leading(str_t str, str_t charset, str_cmp_flags_t flags);
/* Returns a view with trailing bytes from charset removed. */
str_t
str_trim_trailing(str_t str, str_t charset, str_cmp_flags_t flags);
/* Returns a view with matching leading and trailing bytes removed. */
str_t
str_trim(str_t str, str_t charset, str_cmp_flags_t flags);
/* Returns a view with leading ASCII whitespace removed. */
str_t
str_trim_leading_space(str_t str);
/* Returns a view with trailing ASCII whitespace removed. */
str_t
str_trim_trailing_space(str_t str);
/* Returns a view with leading and trailing ASCII whitespace removed. */
str_t
str_trim_space(str_t str);
/* Returns a view with prefix removed when it matches. */
str_t
str_trim_prefix(str_t str, str_t prefix, str_cmp_flags_t flags);
/* Returns a view with suffix removed when it matches. */
str_t
str_trim_suffix(str_t str, str_t suffix, str_cmp_flags_t flags);
/* Returns the part before the first byte found in charset. */
str_t
str_trim_comment(str_t str, str_t charset, str_cmp_flags_t flags);

/* Parsing */

/* Parses a strict signed decimal integer with no whitespace or trailing bytes. */
bool
str_parse_s64(str_t str, int64_t *value);
/* Parses a strict signed 32-bit decimal integer. */
bool
str_parse_int(str_t str, int *value);
/* Parses a strict unsigned 32-bit decimal integer. */
bool
str_parse_u32(str_t str, uint32_t *value);
/* Parses a strict unsigned 16-bit decimal integer. */
bool
str_parse_u16(str_t str, uint16_t *value);
/* Parses a strict unsigned 8-bit decimal integer. */
bool
str_parse_u8(str_t str, uint8_t *value);
/* Parses a strict decimal float with an optional exponent. NaN, infinity, and trailing bytes are rejected. */
bool
str_parse_float(str_t str, float *value);
/* Parses true, yes, on, false, no, or off without case sensitivity. Other values return default_val. */
bool
str_parse_bool(str_t s, bool default_val);

/* ====================================================== PATH ====================================================== */

/*
 * Returns a view of the final path component.
 * Example: /foo/bar.txt --> bar.txt
 */
str_t
path_base(str_t path);
/*
 * Returns a view of the path before its final separator.
 * A path with no separator returns an empty view.
 * Example: /foo/bar.txt --> /foo
 */
str_t
path_dir(str_t path);
/*
 * Returns a view of the final component's extension without the dot, or STR_NULL when absent.
 * Example: /foo/bar.txt -> txt
 */
str_t
path_get_ext(str_t path);
/*
 * Returns a view with the final extension removed.
 * Example: /foo/bar.txt -> /foo/bar
 */
str_t
path_trim_ext(str_t path);
/*
 * Replaces both slash kinds in writable path data and returns the same view.
 * Example (with '\\'): /foo/bar.txt -> \\foo\\bar.txt
 */
str_t
path_replace_slashes_inplace(str_t path, char ch);

/* Compares Windows paths without case or slash direction differences and ignores trailing separators. */
bool
path_equal(str_t a, str_t b);
/* Checks whether prefix is a complete leading path, not just a byte prefix. */
bool
path_has_prefix(str_t path, str_t prefix);
/*
 * Returns a view relative to base when path is inside base. Comparison ignores case and slash direction.
 * Example: path=/foo/bar.txt base=/foo --> bar.txt
 */
bool
path_make_relative(str_t path, str_t base, str_t *out);
/* Copies a relative path into an arena and normalizes separators to forward slashes. */
str_t
path_push_relative(mod_arena_t arena, str_t path, str_t base);

/* Checks for a leading slash or a Windows drive-root path. */
bool
path_is_abs(str_t path);

/*
 * Splits a path on both slash kinds.
 * Returned part strings remain views of path.
 * Example: /foo/bar.txt --> [foo, bar.txt]
 */
str_list_t
path_push_parts(mod_arena_t arena, str_t path);

/* Returns the directory containing the current DLL, allocated in the supplied arena. */
str_t
path_module_dir(mod_arena_t perm);

/*
 * Joins two path parts into an arena string and normalizes separators.
 * The result is not guaranteed to be null-terminated.
 * Example: a:/foo/bar, b:baz.txt --> /foo/bar/baz.txt
 */
str_t
path_join(mod_arena_t arena, str_t a, str_t b);

/* ====================================================== FILE ====================================================== */

typedef uint8_t dir_entry_kind_t;
enum {
  DIR_ENTRY_FILE,
  DIR_ENTRY_DIR,
};

typedef struct dir_entry_s dir_entry_t;
struct dir_entry_s {
  str_t            name; // filename only, no path
  str_t            path; // full path
  dir_entry_kind_t kind;
  uint64_t         size; // file size in bytes (0 for dirs)
};

typedef struct dir_entry_list_s dir_entry_list_t;
struct dir_entry_list_s {
  dir_entry_t *items;
  int          count;
};

/* Returns true when path exists and is a regular file. */
bool
file_exists(str_t path);
/* Returns a file's size in bytes, or zero on failure and for empty files. */
uint64_t
file_size(str_t path);

/* Reads a non-empty file into an arena. The result is raw bytes with a null terminator; empty files return STR_NULL. */
str_t
file_read_all(str_t path, mod_arena_t arena);
/* Creates or truncates a file and writes all supplied bytes. */
bool
file_write(str_t s, str_t path);
/* Creates or truncates a file and writes list items separated by LF, with no final LF. */
bool
file_write_lines(str_list_t lines, str_t path);
/* Copies a file and overwrites an existing destination. */
bool
file_copy(str_t dst, str_t src);
/* Deletes a file and returns whether the OS call succeeded. */
bool
file_delete(str_t path);

/* Returns true when path exists and is a directory. */
bool
dir_exists(str_t path);

/* Creates one directory level. Existing directories count as success. */
bool
dir_create(str_t path);

/* Creates a directory and each missing parent directory. */
bool
dir_create_recursive(str_t path);

/* Lists a directory into an arena, excluding the current and parent entries. */
dir_entry_list_t
dir_list(str_t path, mod_arena_t arena);

/* Lists entries matching a Windows wildcard pattern such as *.dll. */
dir_entry_list_t
dir_list_filter(str_t path, str_t pattern, mod_arena_t arena);

/* Deletes an empty directory. */
bool
dir_delete(str_t path);

/* Deletes child files and directories, then deletes the requested directory. */
bool
dir_delete_recursive(str_t path);

/* =================================================== INI PARSER =================================================== */

typedef struct ini_section_s ini_section_t;
struct ini_section_s {
  ini_section_t *next;
  str_t          name;
  str_t          arg;
  str_list_t     lines;
};

typedef struct ini_section_list_s ini_section_list_t;
struct ini_section_list_s {
  ini_section_t *first;
  ini_section_t *last;
  int            count;
};

/* Checks whether a line starts with [ and ends with ]. */
bool
ini_is_line_section(str_t line);
/* Parses [name] or [name.arg] into trimmed string views. */
bool
ini_parse_section_header(str_t line, str_t *out_name, str_t *out_arg);

/* Builds copied section records from an array of lines. Lines before the first valid section are ignored. */
ini_section_list_t
ini_parse_sections(mod_arena_t arena, str_array_t lines);

/* Splits the first key=value pair into trimmed string views. The key must not be empty. */
bool
ini_parse_kv(str_t line, str_t *out_key, str_t *out_value);
/* Splits INI text, removes semicolon comments, trims whitespace, and deep-copies the resulting lines. */
str_array_t
ini_split_lines(mod_arena_t arena, str_t text);

#ifdef __cplusplus
}
#endif

#endif /* MOD_SDK_H */
