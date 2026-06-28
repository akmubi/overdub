#include "str.h"
#include "arena.h"

#include "vendor_stb.h"

uint64_t
calc_cstr_len(const char *s)
{
  if (!s) {
    return 0;
  }

  const char *p = s;
  while (*p) {
    ++p;
  }

  return (uint64_t)(p - s);
}

uint64_t
calc_cstr_len_with_cap(const char *s, uint64_t cap)
{
  if (!s || cap == 0) {
    return 0;
  }

  const char *p   = s + 0;
  const char *end = s + cap;
  while (p < end && *p) {
    ++p;
  }

  return (uint64_t)(p - s);
}

uint64_t
calc_wstr_len(const wchar_t *s)
{
  if (!s) {
    return 0;
  }

  const wchar_t *p = s;
  while (*p) {
    ++p;
  }

  return (uint64_t)(p - s);
}

uint64_t
calc_wstr_len_with_cap(const wchar_t *s, uint64_t cap)
{
  if (!s || cap == 0) {
    return 0;
  }

  const wchar_t *p   = s + 0;
  const wchar_t *end = s + cap;

  while (p < end && *p) {
    ++p;
  }

  return (uint64_t)(p - s);
}

str_t
str_make(void *data, uint64_t size)
{
  str_t result = {0};
  if (data && size > 0) {
    result = (str_t){
      .data = data,
      .len  = size,
    };
  }
  return result;
}

str_t
str_from_cstr(const char *cstr)
{
  str_t    result = {0};
  uint64_t len    = calc_cstr_len(cstr);
  if (len > 0) {
    result = (str_t){
      .data = (uint8_t *)cstr,
      .len  = len,
    };
  }
  return result;
}

str_t
str_from_cstr_with_cap(const char *cstr, uint64_t cap)
{
  str_t    result = {0};
  uint64_t len    = calc_cstr_len_with_cap(cstr, cap);
  if (len > 0) {
    result = (str_t){
      .data = (uint8_t *)cstr,
      .len  = len,
    };
  }
  return result;
}

str16_t
str16_make(uint16_t *data, uint64_t size)
{
  str16_t result = {0};
  if (data && size > 0) {
    result = (str16_t){
      .data = data,
      .len  = size,
    };
  }
  return result;
}

str16_t
str16_from_wstr(const wchar_t *wstr)
{
  str16_t  result = {0};
  uint64_t len    = calc_wstr_len(wstr);
  if (len > 0) {
    result = (str16_t){
      .data = (uint16_t *)wstr,
      .len  = len,
    };
  }
  return result;
}

str16_t
str16_from_wstr_with_cap(const wchar_t *wstr, uint64_t cap)
{
  str16_t  result = {0};
  uint64_t len    = calc_wstr_len_with_cap(wstr, cap);
  if (len > 0) {
    result = (str16_t){
      .data = (uint16_t *)wstr,
      .len  = len,
    };
  }
  return result;
}

str_t
str_slice(str_t str, uint64_t start_idx, uint64_t end_idx)
{
  if (str.data && str.len > 0) {
    if (end_idx > str.len) {
      end_idx = str.len;
    }

    if (start_idx > str.len) {
      start_idx = str.len;
    }

    if (start_idx > end_idx) {
      uint64_t tmp = start_idx;

      start_idx = end_idx;
      end_idx   = tmp;
    }

    str.len   = end_idx - start_idx;
    str.data += start_idx;
  }
  return str;
}

static int
ascii_compare(uint8_t a, uint8_t b, str_cmp_flags_t flags)
{
  if (flags & STR_CMP_FLAG_IGNORE_SLASHES) {
    a = ascii_to_forward_slash(a);
    b = ascii_to_forward_slash(b);
  }

  if (flags & STR_CMP_FLAG_IGNORE_CASE) {
    a = ascii_to_lower(a);
    b = ascii_to_lower(b);
  }

  return (int)a - (int)b;
}

bool
str_has_prefix(str_t s, str_t prefix, str_cmp_flags_t flags)
{
  if (prefix.len > s.len) {
    return false;
  }

  for (uint64_t i = 0; i < prefix.len; ++i) {
    uint8_t ca = s.data[i];
    uint8_t cb = prefix.data[i];

    if (!ascii_equal(ca, cb, flags)) {
      return false;
    }
  }

  return true;
}

bool
str_has_suffix(str_t s, str_t suffix, str_cmp_flags_t flags)
{
  if (suffix.len > s.len) {
    return false;
  }

  uint64_t offset = s.len - suffix.len;
  for (uint64_t i = 0; i < suffix.len; ++i) {
    uint8_t ca = s.data[offset + i];
    uint8_t cb = suffix.data[i];

    if (!ascii_equal(ca, cb, flags)) {
      return false;
    }
  }

  return true;
}

int
str_compare(str_t a, str_t b, str_cmp_flags_t flags)
{
  uint64_t min_len = MIN_VAL(a.len, b.len);

  for (uint64_t i = 0; i < min_len; ++i) {
    int result = ascii_compare(a.data[i], b.data[i], flags);
    if (result != 0) {
      return result;
    }
  }

  if (a.len < b.len) {
    return -1;
  } else if (a.len > b.len) {
    return 1;
  }
  return 0;
}

bool
str_equal(str_t a, str_t b, str_cmp_flags_t flags)
{
  if (a.len != b.len) {
    return false;
  }

  if (a.len == 0) {
    return true;
  }

  for (uint64_t i = 0; i < a.len; ++i) {
    uint8_t ca = a.data[i];
    uint8_t cb = b.data[i];

    if (!ascii_equal(ca, cb, flags)) {
      return false;
    }
  }

  return true;
}

bool
str_equal_icase(str_t a, str_t b)
{
  return str_equal(a, b, STR_CMP_FLAG_IGNORE_CASE);
}

bool
str_find(str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx)
{
  if (sub.len == 0) {
    if (idx) {
      *idx = 0;
    }
    return true;
  }

  if (sub.len > s.len) {
    return false;
  }

  uint64_t last = s.len - sub.len;
  for (uint64_t i = 0; i <= last; ++i) {
    str_t win = str_make(&s.data[i], sub.len);
    if (str_equal(win, sub, flags)) {
      if (idx) {
        *idx = i;
      }
      return true;
    }
  }

  return false;
}

bool
str_rfind(str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx)
{
  if (sub.len == 0) {
    if (idx) {
      *idx = 0;
    }
    return true;
  }

  if (sub.len > s.len) {
    return false;
  }

  uint64_t last = s.len - sub.len;
  for (uint64_t i = 0; i <= last; ++i) {
    uint64_t j   = last - i;
    str_t    win = str_make(&s.data[j], sub.len);
    if (str_equal(win, sub, flags)) {
      if (idx) {
        *idx = j;
      }
      return true;
    }
  }
  return false;
}

bool
str_find_any(str_t s, str_t charset, str_cmp_flags_t flags, uint64_t *idx)
{
  if (charset.len == 0) {
    if (idx) {
      *idx = 0;
    }
    return true;
  }

  for (uint64_t i = 0; i < s.len; ++i) {
    for (uint64_t j = 0; j < charset.len; ++j) {
      if (ascii_equal(s.data[i], charset.data[j], flags)) {
        if (idx) {
          *idx = i;
        }
        return true;
      }
    }
  }
  return false;
}

bool
str_rfind_any(str_t s, str_t charset, str_cmp_flags_t flags, uint64_t *idx)
{
  if (charset.len == 0) {
    if (idx) {
      *idx = 0;
    }
    return true;
  }

  for (uint64_t i = 0; i < s.len; ++i) {
    for (uint64_t j = 0; j < charset.len; ++j) {
      if (ascii_equal(s.data[s.len - i - 1], charset.data[j], flags)) {
        if (idx) {
          *idx = s.len - i - 1;
        }
        return true;
      }
    }
  }
  return false;
}

bool
str_has_char(str_t s, char c, str_cmp_flags_t flags)
{
  for (uint64_t i = 0; i < s.len; ++i) {
    if (ascii_equal(s.data[i], c, flags)) {
      return true;
    }
  }
  return false;
}

str_t
str_push_copy(arena_t *arena, str_t str)
{
  str_t result = {
    .len  = str.len,
    .data = ARENA_PUSH_ARRAY(arena, uint8_t, str.len + 1),
  };

  if (result.data) {
    if (result.len > 0) {
      mem_copy(result.data, str.data, str.len);
    }
    result.data[result.len] = '\0';
  }
  return result;
}

uint64_t
str_write_vfmt(void *buf, uint64_t cap, const char *fmt, va_list args)
{
  va_list args_copy;
  va_copy(args_copy, args);

  int written;
  if (!buf || cap == 0) {
    written = stbsp_vsnprintf(NULL, 0, fmt, args_copy);
  } else {
    written = stbsp_vsnprintf((char *)buf, (size_t)cap, fmt, args_copy);
  }

  va_end(args_copy);

  return written >= 0 ? (uint64_t)written : 0;
}

uint64_t
str_write_fmt(void *buf, uint64_t cap, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  uint64_t written = str_write_vfmt(buf, cap, fmt, args);
  va_end(args);
  return written;
}

str_t
str_push_vfmt(arena_t *arena, const char *fmt, va_list args)
{
  va_list args2;
  va_copy(args2, args);
  int needed = stbsp_vsnprintf(NULL, 0, fmt, args2);
  va_end(args2);

  if (needed < 0) {
    return (str_t){0};
  }

  str_t result = {
    .len  = (uint64_t)needed,
    .data = ARENA_PUSH_ARRAY(arena, uint8_t, needed + 1),
  };

  if (result.data) {
    if (needed > 0) {
      stbsp_vsnprintf((char *)result.data, needed + 1, fmt, args);
    }
    result.data[needed] = '\0';
  }
  return result;
}

str_t
str_push_fmt(arena_t *arena, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  str_t result = str_push_vfmt(arena, fmt, args);
  va_end(args);
  return result;
}

str_t
str_push_fill_byte(arena_t *arena, uint64_t size, uint8_t byte)
{
  str_t result = {
    .data = ARENA_PUSH_ARRAY(arena, uint8_t, size + 1),
    .len  = size,
  };

  if (result.data) {
    if (result.len > 0) {
      mem_set(result.data, byte, size);
    }
    result.data[result.len] = '\0';
  }
  return result;
}

str_t
str_push_hex(arena_t *arena, void *data, uint64_t size)
{
  uint64_t hex_size = size * 2;
  str_t    result   = {
    .data = ARENA_PUSH_ARRAY(arena, uint8_t, hex_size + 1),
    .len  = hex_size,
  };

  if (result.data) {
    if (result.len > 0) {
      uint8_t *buf = (uint8_t *)data;
      for (uint64_t i = 0; i < size; ++i) {
        uint8_t lo = (buf[i] >> 0) & 0x0F;
        uint8_t hi = (buf[i] >> 4) & 0x0F;

        result.data[2 * i + 0] = nibble_to_hex(hi);
        result.data[2 * i + 1] = nibble_to_hex(lo);
      }
    }

    result.data[result.len] = '\0';
  }
  return result;
}

str_t
str_push_concat(arena_t *arena, str_t a, str_t b)
{
  uint64_t len    = a.len + b.len;
  str_t    result = {
    .data = ARENA_PUSH_ARRAY(arena, uint8_t, len + 1),
    .len  = len,
  };

  if (result.data) {
    if (result.len > 0) {
      mem_copy(result.data + 0, a.data, a.len);
      mem_copy(result.data + a.len, b.data, b.len);
    }
    result.data[result.len] = '\0';
  }

  return result;
}

str_list_t
str_list_copy(arena_t *arena, str_list_t src)
{
  str_list_t dst = {0};
  for (str_node_t *node = src.first; node; node = node->next) {
    str_list_push_copy(arena, &dst, node->str);
  }
  return dst;
}

void
str_list_push_node(str_list_t *list, str_node_t *node)
{
  QUEUE_PUSH(list->first, list->last, node);
  list->count       += 1;
  list->total_bytes += node->str.len;
}

void
str_list_push_node_front(str_list_t *list, str_node_t *node)
{
  QUEUE_PUSH_FRONT(list->first, list->last, node);
  list->count       += 1;
  list->total_bytes += node->str.len;
}

void
str_list_push(arena_t *arena, str_list_t *list, str_t str)
{
  str_node_t *node = ARENA_PUSH_ARRAY_ZERO(arena, str_node_t, 1);
  if (node) {
    node->str = str;
    str_list_push_node(list, node);
  }
}

void
str_list_push_copy(arena_t *arena, str_list_t *list, str_t str)
{
  str_list_push(arena, list, str_push_copy(arena, str));
}

void
str_list_push_fmt(arena_t *arena, str_list_t *list, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  str_t s = str_push_vfmt(arena, fmt, args);
  va_end(args);
  str_list_push(arena, list, s);
}

void
str_list_push_front(arena_t *arena, str_list_t *list, str_t str)
{
  str_node_t *node = ARENA_PUSH_ARRAY_ZERO(arena, str_node_t, 1);
  if (node) {
    node->str = str;
    str_list_push_node_front(list, node);
  }
}

void
str_list_push_front_fmt(arena_t *arena, str_list_t *list, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  str_t s = str_push_vfmt(arena, fmt, args);
  va_end(args);
  str_list_push_front(arena, list, s);
}

void
str_list_concat_inplace(str_list_t *dst, str_list_t *src)
{
  if (!src->first) {
    return;
  }

  if (!dst->first) {
    *dst = *src;
  } else {
    dst->last->next   = src->first;
    dst->last         = src->last;
    dst->count       += src->count;
    dst->total_bytes += src->total_bytes;
  }
  mem_zero(src, sizeof(*src));
}

str_list_t
str_split(arena_t *arena, str_t str, uint64_t split_count, str_t *splits)
{
  str_list_t list = {0};

  uint64_t split_start = 0;
  for (uint64_t i = 0; i < str.len; ++i) {
    bool was_split = false;
    for (uint64_t split_idx = 0; split_idx < split_count; ++split_idx) {
      bool match = false;
      if (i + splits[split_idx].len <= str.len) {
        match = true;
        for (uint64_t split_i = 0; split_i < splits[split_idx].len && i + split_i < str.len; ++split_i) {
          if (splits[split_idx].data[split_i] != str.data[i + split_i]) {
            match = false;
            break;
          }
        }
      }

      if (match) {
        str_t split_string = str_make(str.data + split_start, i - split_start);
        str_list_push(arena, &list, split_string);
        split_start  = i + splits[split_idx].len;
        i           += splits[split_idx].len - 1;
        was_split    = true;
        break;
      }
    }

    if (was_split == 0 && i == str.len - 1) {
      str_t split_string = str_make(str.data + split_start, i + 1 - split_start);
      str_list_push(arena, &list, split_string);
      break;
    }
  }

  return list;
}

str_list_t
str_split_lines(arena_t *arena, str_t str)
{
  str_list_t list = {0};

  uint64_t start_idx = 0;
  for (uint64_t i = 0; i < str.len; ++i) {
    bool is_eof = (i == str.len - 1);
    char c      = str.data[i];

    bool is_newline = (c == '\r' || c == '\n');

    if (is_newline || is_eof) {
      // for newlines, end before the newline char; for EOF, include the last char
      uint64_t end_idx = is_newline ? i : i + 1;

      str_t line = str_slice(str, start_idx, end_idx);
      line       = str_trim_trailing_space(line);
      str_list_push(arena, &list, line);

      // skip \r\n or \n\r pairs
      if (is_newline && (i + 1) < str.len) {
        char next = str.data[i + 1];
        if ((c == '\r' && next == '\n') || (c == '\n' && next == '\r')) {
          i += 1;
        }
      }

      start_idx = i + 1;
    }
  }

  return list;
}

str_t
str_list_join(arena_t *arena, str_list_t list, str_t pre, str_t sep, str_t post)
{
  uint64_t seps  = (list.count > 1) ? (list.count - 1) : 0;
  uint64_t total = (list.total_bytes + pre.len + seps * sep.len + post.len);

  str_t result = {
    .data = ARENA_PUSH_ARRAY(arena, uint8_t, total + 1),
    .len  = total,
  };

  if (result.data) {
    if (result.len > 0) {
      uint8_t *ptr = result.data;
      mem_copy(ptr, pre.data, pre.len);
      ptr += pre.len;

      for (str_node_t *node = list.first; node; node = node->next) {
        mem_copy(ptr, node->str.data, node->str.len);
        ptr += node->str.len;

        if (node != list.last) {
          mem_copy(ptr, sep.data, sep.len);
          ptr += sep.len;
        }
      }

      mem_copy(ptr, post.data, post.len);
      ptr += post.len;
    }

    result.data[result.len] = '\0';
  }
  return result;
}

str_t
str_list_join_lines(arena_t *arena, str_list_t list)
{
  return str_list_join(arena, list, STR_NULL, STR_LIT("\n"), STR_NULL);
}

str_array_t
str_array_make(str_t *items, int count)
{
  return (str_array_t){
    .items = items,
    .count = count,
  };
}

str_array_t
str_array_copy(arena_t *arena, str_array_t src)
{
  ASSERT((!src.items && src.count == 0) || (src.items && src.count > 0));

  str_array_t dst = {
    .count = src.count,
    .items = ARENA_PUSH_ARRAY(arena, str_t, src.count),
  };

  if (dst.items) {
    for (uint64_t i = 0; i < src.count; ++i) {
      dst.items[i] = str_push_copy(arena, src.items[i]);
    }
  }
  return dst;
}

str_array_t
str_array_copy_from_list(arena_t *arena, str_list_t list)
{
  return str_array_from_list(arena, str_list_copy(arena, list));
}

str_array_t
str_array_from_list(arena_t *arena, str_list_t list)
{
  str_array_t array = {
    .count = list.count,
    .items = ARENA_PUSH_ARRAY(arena, str_t, list.count),
  };

  if (array.items) {
    uint64_t i = 0;
    for (str_node_t *node = list.first; node != 0; node = node->next, ++i) {
      array.items[i] = node->str;
    }
  }
  return array;
}

bool
str_array_contains(str_array_t array, str_t s, str_cmp_flags_t flags)
{
  for (uint64_t i = 0; i < array.count; ++i) {
    if (str_equal(array.items[i], s, flags)) {
      return true;
    }
  }
  return false;
}

bool
str_array_find(str_array_t array, str_t s, str_cmp_flags_t flags, uint64_t *idx)
{
  for (uint64_t i = 0; i < array.count; ++i) {
    if (str_equal(array.items[i], s, flags)) {
      if (idx) {
        *idx = i;
      }
      return true;
    }
  }
  return false;
}

bool
str_list_contains(str_list_t list, str_t s, str_cmp_flags_t flags)
{
  for (str_node_t *node = list.first; node; node = node->next) {
    if (str_equal(node->str, s, flags)) {
      return true;
    }
  }
  return false;
}

char *
str_push_cstr(arena_t *arena, str_t s)
{
  char *cstr = ARENA_PUSH_ARRAY(arena, char, s.len + 1);
  if (cstr) {
    if (s.len > 0) {
      mem_copy(cstr, s.data, s.len * sizeof(char));
    }
    cstr[s.len] = '\0';
  }
  return cstr;
}

wchar_t *
str16_push_wstr(arena_t *arena, str16_t s)
{
  wchar_t *wstr = ARENA_PUSH_ARRAY(arena, wchar_t, s.len + 1);
  if (wstr) {
    if (s.len > 0) {
      mem_copy(wstr, s.data, s.len * sizeof(wchar_t));
    }
    wstr[s.len] = '\0';
  }
  return wstr;
}

uint64_t
str_write_cstr(str_t s, char *buf, uint64_t cap)
{
  if (!buf || cap == 0 || str_is_empty(s)) {
    return 0;
  }

  uint64_t len = MIN_VAL(s.len + 1, cap);
  mem_copy(buf, s.data, len - 1);
  buf[len - 1] = '\0';
  return len;
}

uint64_t
str_write_data(str_t s, char *buf, uint64_t cap)
{
  if (!buf || cap == 0 || str_is_empty(s)) {
    return 0;
  }

  uint64_t len = MIN_VAL(s.len, cap);
  mem_copy(buf, s.data, len);
  return len;
}

str_t
str_to_upper(arena_t *arena, str_t str)
{
  str_t result = str_push_copy(arena, str);
  if (!str_is_empty(result)) {
    for (uint64_t i = 0; i < str.len; ++i) {
      result.data[i] = ascii_to_upper(str.data[i]);
    }
  }
  return result;
}

str_t
str_to_lower(arena_t *arena, str_t str)
{
  str_t result = str_push_copy(arena, str);
  if (!str_is_empty(result)) {
    for (uint64_t i = 0; i < str.len; ++i) {
      result.data[i] = ascii_to_lower(str.data[i]);
    }
  }
  return result;
}

str_t
str_trim_leading(str_t str, str_t charset, str_cmp_flags_t flags)
{
  uint64_t start_idx = str.len;
  for (uint64_t i = 0; i < str.len; ++i) {
    if (!str_has_char(charset, str.data[i], flags)) {
      start_idx = i;
      break;
    }
  }

  return str_slice(str, start_idx, str.len);
}

str_t
str_trim_trailing(str_t str, str_t charset, str_cmp_flags_t flags)
{
  uint64_t end_idx = 0;
  for (uint64_t i = 0; i < str.len; ++i) {
    uint64_t j = str.len - 1 - i; // reverse order
    if (!str_has_char(charset, str.data[j], flags)) {
      end_idx = j + 1;
      break;
    }
  }

  return str_slice(str, 0, end_idx);
}

str_t
str_trim(str_t str, str_t charset, str_cmp_flags_t flags)
{
  return str_trim_trailing(str_trim_leading(str, charset, flags), charset, flags);
}

str_t
str_trim_leading_space(str_t str)
{
  return str_trim_leading(str, STR_LIT(" \r\t\f\v\n"), 0);
}

str_t
str_trim_trailing_space(str_t str)
{
  return str_trim_trailing(str, STR_LIT(" \r\t\f\v\n"), 0);
}

str_t
str_trim_space(str_t str)
{
  return str_trim(str, STR_LIT(" \r\t\f\v\n"), 0);
}

str_t
str_trim_prefix(str_t str, str_t prefix, str_cmp_flags_t flags)
{
  str_t result = str;
  if (str_has_prefix(str, prefix, flags)) {
    result = str_slice(str, prefix.len, str.len);
  }
  return result;
}

str_t
str_trim_suffix(str_t str, str_t suffix, str_cmp_flags_t flags)
{
  str_t result = str;
  if (str_has_suffix(str, suffix, flags)) {
    result = str_slice(str, 0, str.len - suffix.len);
  }
  return result;
}

/* str_trim_comment(STR_LIT("example # this is a comment"), STR_LIT("#"), 0) --> STR_LIT("example ") */
str_t
str_trim_comment(str_t str, str_t charset, str_cmp_flags_t flags)
{
  uint64_t end_idx = str.len;
  for (uint64_t i = 0; i < str.len; ++i) {
    if (str_has_char(charset, str.data[i], flags)) {
      end_idx = i;
      break;
    }
  }

  return str_slice(str, 0, end_idx);
}

bool
str_parse_s64(str_t str, int64_t *value)
{
  if (str_is_empty(str)) {
    return false;
  }

  uint64_t i    = 0;
  int      sign = 1;

  if (str.data[0] == '-') {
    sign = -1;
    i    = 1;
  } else if (str.data[0] == '+') {
    i = 1;
  }

  if (i == str.len) {
    return false;
  }

  int64_t result = 0;
  for (; i < str.len; ++i) {
    uint8_t c = str.data[i];
    if (!ascii_is_num(c)) {
      return false;
    }

    int digit = c - '0';

    /* overflow check before multiply + add */
    if (result > (INT64_MAX - digit) / 10) {
      return false;
    }
    result = result * 10 + digit;
  }

  result *= sign;

  *value = result;
  return true;
}

bool
str_parse_int(str_t str, int *value)
{
  int64_t int_value = 0;
  if (!str_parse_s64(str, &int_value)) {
    return false;
  }

  if (!IN_RANGE(int_value, INT32_MIN, INT32_MAX)) {
    return false;
  }

  *value = (int)int_value;
  return true;
}

bool
str_parse_u32(str_t str, uint32_t *value)
{
  int64_t int_value = 0;
  if (!str_parse_s64(str, &int_value)) {
    return false;
  }

  if (!IN_RANGE(int_value, 0, UINT32_MAX)) {
    return false;
  }

  *value = (uint32_t)int_value;
  return true;
}

bool
str_parse_u16(str_t str, uint16_t *value)
{
  int64_t int_value = 0;
  if (!str_parse_s64(str, &int_value)) {
    return false;
  }

  if (!IN_RANGE(int_value, 0, UINT16_MAX)) {
    return false;
  }

  *value = (uint16_t)int_value;
  return true;
}

bool
str_parse_u8(str_t str, uint8_t *value)
{
  int64_t int_value = 0;
  if (!str_parse_s64(str, &int_value)) {
    return false;
  }

  if (!IN_RANGE(int_value, 0, UINT8_MAX)) {
    return false;
  }

  *value = (uint8_t)int_value;
  return true;
}

bool
str_parse_float(str_t str, float *value)
{
  if (str_is_empty(str)) {
    return false;
  }

  uint64_t i    = 0;
  double   sign = 1.0;

  if (str.data[0] == '-') {
    sign = -1.0;
    i    = 1;
  } else if (str.data[0] == '+') {
    i = 1;
  }

  if (i == str.len) {
    return false;
  }

  bool   has_digit = false;
  double result    = 0.0;

  /* integer part */
  for (; i < str.len && ascii_is_num(str.data[i]); ++i) {
    result    = result * 10.0 + (str.data[i] - '0');
    has_digit = true;
  }

  /* fractional part */
  if (i < str.len && str.data[i] == '.') {
    i           += 1;
    double frac  = 0.1;
    for (; i < str.len && ascii_is_num(str.data[i]); ++i) {
      result    += (str.data[i] - '0') * frac;
      frac      *= 0.1;
      has_digit  = true;
    }
  }

  if (!has_digit) {
    return false;
  }

  /* exponent part */
  if (i < str.len && (str.data[i] == 'e' || str.data[i] == 'E')) {
    i += 1;

    int exp_sign = 1;
    if (i < str.len && str.data[i] == '-') {
      exp_sign  = -1;
      i        += 1;
    } else if (i < str.len && str.data[i] == '+') {
      i += 1;
    }

    if (i == str.len || !ascii_is_num(str.data[i])) {
      return false;
    }

    int exp = 0;
    for (; i < str.len && ascii_is_num(str.data[i]); ++i) {
      if (exp > 300) {
        return false; // prevent absurd exponents
      }
      exp = exp * 10 + (str.data[i] - '0');
    }

    double pow10 = 1.0;
    for (int e = 0; e < exp; ++e) {
      pow10 *= 10.0;
    }

    if (exp_sign > 0) {
      result *= pow10;
    } else {
      result /= pow10;
    }
  }

  /* trailing junk */
  if (i != str.len) {
    return false;
  }

  result *= sign;

  /* range check - reject inf */
  float f = (float)result;
  if (!IN_RANGE(f, -FLT_MAX, FLT_MAX) || f != f) {
    return false;
  }

  *value = f;
  return true;
}

bool
str_parse_bool(str_t s, bool default_val)
{
  if (str_equal_icase(s, STR_LIT("true")) || str_equal_icase(s, STR_LIT("yes")) || str_equal_icase(s, STR_LIT("on"))) {
    return true;
  }

  if (str_equal_icase(s, STR_LIT("false")) || str_equal_icase(s, STR_LIT("no")) || str_equal_icase(s, STR_LIT("off"))) {
    return false;
  }

  return default_val;
}
