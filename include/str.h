#ifndef STR_H
#define STR_H

#include "arena.h"
#include "types.h"

#define STR_CLIT(lit) {.data = (uint8_t *)(ENSURE_STR_LIT(lit)), .len = (sizeof(lit) - 1)}
#define STR_LIT(lit)  (str_t) STR_CLIT(ENSURE_STR_LIT(lit))
#define STR_NULL      (str_t){0}

#define STR_FMT      "%.*s"
#define STR_ARG(str) (int)(str).len, (str).data

#define STR_BOOL(v)  ((v) ? STR_LIT("true") : STR_LIT("false"))

typedef uint32_t str_cmp_flags_t;
enum {
  STR_CMP_FLAG_IGNORE_CASE    = (1 << 0),
  STR_CMP_FLAG_IGNORE_SLASHES = (1 << 1),
};

/* UTF-8 string */
typedef struct str_s str_t;
struct str_s {
  uint8_t *data;
  uint64_t len;
};

/* UTF-16 string */
typedef struct str16_s str16_t;
struct str16_s {
  uint16_t *data;
  uint64_t  len;
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

static inline uint16_t
utf16_ascii_to_lower(uint16_t c)
{
  return (c >= 'A' && c <= 'Z') ? (uint16_t)('a' + (c - 'A')) : c;
}

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

/* length calculation */
uint64_t
calc_cstr_len(const char *cstr);
uint64_t
calc_cstr_len_with_cap(const char *s, uint64_t cap);
uint64_t
calc_wstr_len(const wchar_t *wstr);
uint64_t
calc_wstr_len_with_cap(const wchar_t *s, uint64_t cap);

/* creation */
str_t
str_make(void *data, uint64_t size);
str_t
str_from_cstr(const char *cstr);
str_t
str_from_cstr_with_cap(const char *cstr, uint64_t cap);
str16_t
str16_make(uint16_t *data, uint64_t size);
str16_t
str16_from_wstr(const wchar_t *wstr);
str16_t
str16_from_wstr_with_cap(const wchar_t *wstr, uint64_t cap);
str_t
str_slice(str_t str, uint64_t start_idx, uint64_t end_idx);

/* comparison/search */
bool
str_has_prefix(str_t s, str_t prefix, str_cmp_flags_t flags);
bool
str_has_suffix(str_t s, str_t suffix, str_cmp_flags_t flags);
int
str_compare(str_t a, str_t b, str_cmp_flags_t flags);
bool
str_equal(str_t a, str_t b, str_cmp_flags_t flags);
bool
str_equal_icase(str_t a, str_t b);
bool
str_find(str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx);
bool
str_rfind(str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx);
bool
str_find_any(str_t s, str_t charset, str_cmp_flags_t flags, uint64_t *idx);
bool
str_rfind_any(str_t s, str_t charset, str_cmp_flags_t flags, uint64_t *idx);
bool
str_has_char(str_t s, char c, str_cmp_flags_t flags);

/* allocation */
str_t
str_push_copy(arena_t *arena, str_t str);
uint64_t
str_write_vfmt(void *buf, uint64_t cap, const char *fmt, va_list args);
uint64_t
str_write_fmt(void *buf, uint64_t cap, const char *fmt, ...) ATTR_FORMAT(3, 4);
str_t
str_push_vfmt(arena_t *arena, const char *fmt, va_list args);
str_t
str_push_fmt(arena_t *arena, const char *fmt, ...) ATTR_FORMAT(2, 3);
str_t
str_push_fill_byte(arena_t *arena, uint64_t size, uint8_t byte);
str_t
str_push_concat(arena_t *arena, str_t a, str_t b);
str_t
str_push_hex(arena_t *arena, void *data, uint64_t size);

/* string list/array */
str_list_t
str_list_copy(arena_t *arena, str_list_t src);
void
str_list_push_node(str_list_t *list, str_node_t *n);
void
str_list_push_node_front(str_list_t *list, str_node_t *n);
void
str_list_push(arena_t *arena, str_list_t *list, str_t str);
void
str_list_push_copy(arena_t *arena, str_list_t *list, str_t str);
void
str_list_push_fmt(arena_t *arena, str_list_t *list, const char *fmt, ...) ATTR_FORMAT(3, 4);
void
str_list_push_front(arena_t *arena, str_list_t *list, str_t str);
void
str_list_push_front_fmt(arena_t *arena, str_list_t *list, const char *fmt, ...) ATTR_FORMAT(3, 4);
bool
str_list_contains(str_list_t list, str_t s, str_cmp_flags_t flags);
void
str_list_concat_inplace(str_list_t *dst, str_list_t *src);
str_list_t
str_split(arena_t *arena, str_t str, uint64_t split_count, str_t *splits);
str_list_t
str_split_lines(arena_t *arena, str_t str);
str_t
str_list_join(arena_t *arena, str_list_t list, str_t pre, str_t sep, str_t post);
str_t
str_list_join_lines(arena_t *arena, str_list_t list);
str_array_t
str_array_make(str_t *items, int count);
str_array_t
str_array_copy(arena_t *arena, str_array_t a);
str_array_t
str_array_copy_from_list(arena_t *arena, str_list_t list);
str_array_t
str_array_from_list(arena_t *arena, str_list_t list);
bool
str_array_contains(str_array_t array, str_t s, str_cmp_flags_t flags);
bool
str_array_find(str_array_t array, str_t s, str_cmp_flags_t flags, uint64_t *idx);

/* UTF-8 <-> UTF-16 conversions */
str_t
str_from_str16(arena_t *arena, str16_t in);
str16_t
str16_from_str(arena_t *arena, str_t in);

uint64_t
str16_utf8_len(str16_t in);
uint64_t
str16_write_utf8(uint8_t *buf, uint64_t max_len, str16_t in);

/* C-string conversion */
char *
str_push_cstr(arena_t *arena, str_t s);
wchar_t *
str16_push_wstr(arena_t *arena, str16_t s);

uint64_t
str_write_cstr(str_t s, char *buf, uint64_t cap);
uint64_t
str_write_data(str_t s, char *buf, uint64_t cap);

/* mutations */
str_t
str_to_upper(arena_t *arena, str_t str);
str_t
str_to_lower(arena_t *arena, str_t str);

/* trimming */
str_t
str_trim_leading(str_t str, str_t charset, str_cmp_flags_t flags);
str_t
str_trim_trailing(str_t str, str_t charset, str_cmp_flags_t flags);
str_t
str_trim(str_t str, str_t charset, str_cmp_flags_t flags);
str_t
str_trim_leading_space(str_t str);
str_t
str_trim_trailing_space(str_t str);
str_t
str_trim_space(str_t str);
str_t
str_trim_prefix(str_t str, str_t prefix, str_cmp_flags_t flags);
str_t
str_trim_suffix(str_t str, str_t suffix, str_cmp_flags_t flags);
str_t
str_trim_comment(str_t str, str_t charset, str_cmp_flags_t flags);

/* parsing */
bool
str_parse_s64(str_t str, int64_t *value);
bool
str_parse_int(str_t str, int *value);
bool
str_parse_u32(str_t str, uint32_t *value);
bool
str_parse_u16(str_t str, uint16_t *value);
bool
str_parse_u8(str_t str, uint8_t *value);
bool
str_parse_float(str_t str, float *value);
bool
str_parse_bool(str_t s, bool default_val);

#endif /* STR_H */
