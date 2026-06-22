#include "path.h"

#include <windows.h>

str_t
path_base(str_t path)
{
  uint64_t last_slash_idx = 0;
  if (str_rfind(path, STR_LIT("/"), STR_CMP_FLAG_IGNORE_SLASHES, &last_slash_idx)) {
    path = str_slice(path, last_slash_idx + 1, path.len);
  }
  return path;
}

str_t
path_dir(str_t path)
{
  uint64_t last_slash_idx = 0;
  str_rfind(path, STR_LIT("/"), STR_CMP_FLAG_IGNORE_SLASHES, &last_slash_idx);
  return str_slice(path, 0, last_slash_idx);
}

str_t
path_get_ext(str_t path)
{
  uint64_t dot_idx = 0;

  path = path_base(path);
  if (str_rfind(path, STR_LIT("."), 0, &dot_idx)) {
    path = str_slice(path, dot_idx + 1, path.len);
  } else {
    path = STR_NULL;
  }
  return path;
}

str_t
path_trim_ext(str_t path)
{
  uint64_t idx = 0;
  if (str_rfind_any(path, STR_LIT("./"), STR_CMP_FLAG_IGNORE_SLASHES, &idx) && path.data[idx] == '.') {
    path = str_slice(path, 0, idx);
  }
  return path;
}

static bool
path_is_sep(uint8_t c)
{
  return c == '/' || c == '\\';
}

str_t
path_replace_slashes_inplace(str_t path, char ch)
{
  for (uint64_t i = 0; i < path.len; ++i) {
    if (path_is_sep(path.data[i])) {
      path.data[i] = ch;
    }
  }
  return path;
}

static str_t
path_trim_trailing_seps(str_t path)
{
  while (path.len > 0) {
    uint8_t c = path.data[path.len - 1];
    if (!path_is_sep(c)) {
      break;
    }

    /* keep "/" intact */
    if (path.len == 1) {
      break;
    }

    /* keep "C:/" intact */
    if (path.len == 3 &&
        ascii_is_alpha(path.data[0]) &&
        path.data[1] == ':' &&
        path_is_sep(path.data[2])) {
      break;
    }

    path.len -= 1;
  }

  return path;
}

bool
path_equal(str_t a, str_t b)
{
  a = path_trim_trailing_seps(a);
  b = path_trim_trailing_seps(b);
  return str_equal(a, b, STR_CMP_FLAG_IGNORE_CASE|STR_CMP_FLAG_IGNORE_SLASHES);
}

bool
path_has_prefix(str_t path, str_t prefix)
{
  path   = path_trim_trailing_seps(path);
  prefix = path_trim_trailing_seps(prefix);

  if (str_is_empty(prefix)) {
    return true;
  }

  if (prefix.len > path.len) {
    return false;
  }

  if (!str_has_prefix(path, prefix, STR_CMP_FLAG_IGNORE_CASE|STR_CMP_FLAG_IGNORE_SLASHES)) {
    return false;
  }

  /* exact match */
  if (prefix.len == path.len) {
    return true;
  }

  /* next char must be a separator, otherwise "C:/Game" matches "C:/GameX" */
  return path_is_sep(path.data[prefix.len]);
}

bool
path_make_relative(str_t path, str_t base, str_t *out)
{
  if (out) {
    *out = STR_NULL;
  }

  path = path_trim_trailing_seps(path);
  base = path_trim_trailing_seps(base);

  if (!path_has_prefix(path, base)) {
    return false;
  }

  if (base.len == path.len) {
    if (out) {
      *out = STR_NULL;
    }
    return true;
  }

  uint64_t start = base.len;
  while (start < path.len && path_is_sep(path.data[start])) {
    start += 1;
  }

  if (out) {
    *out = str_slice(path, start, path.len);
  }
  return true;
}

str_t
path_push_relative(arena_t *arena, str_t path, str_t base)
{
  str_t rel = STR_NULL;
  if (!path_make_relative(path, base, &rel)) {
    return STR_NULL;
  }

  rel = str_push_copy(arena, rel);
  return path_replace_slashes_inplace(rel, '/');
}

bool
path_is_abs(str_t path)
{
  // /mnt/c/Users/user/AppData
  if (str_has_prefix(path, STR_LIT("/"), STR_CMP_FLAG_IGNORE_SLASHES)) {
    return true;
  }

  // D:\\Users\\user\\AppData
  if (path.len >= 3 && ascii_is_alpha(path.data[0]) && path.data[1] == ':' && path_is_sep(path.data[2])) {
    return true;
  }

  return false;
}

str_list_t
path_push_parts(arena_t *arena, str_t path)
{
  str_t splits[] = {
    STR_LIT("/"),
    STR_LIT("\\"),
  };

  return str_split(arena, path, COUNTOF(splits), splits);
}

str_t
path_module_dir(arena_t *perm)
{
  wchar_t buf[MAX_PATH + 1] = {0};
  HMODULE h_mod              = NULL;

  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCWSTR)path_module_dir,
                     &h_mod);

  DWORD len = GetModuleFileNameW(h_mod, buf, COUNTOF(buf));
  if (len == 0 || len >= COUNTOF(buf)) {
    return (str_t){0};
  }

  str16_t full_path16 = str16_make((uint16_t *)buf, (uint64_t)len);
  str_t   full_path   = str_from_str16(perm, full_path16);
  return path_dir(full_path);
}

str_t
path_join(arena_t *arena, str_t a, str_t b)
{
  str_t result = {0};
  if (str_is_empty(a)) {
    result = str_push_copy(arena, b);
  } else if (str_is_empty(b)) {
    result = str_push_copy(arena, a);
  } else {
    str_t sep = STR_LIT("/");

    if (str_has_suffix(a, sep, STR_CMP_FLAG_IGNORE_SLASHES)) {
      a = str_slice(a, 0, a.len - 1);
    }

    if (str_has_prefix(b, sep, STR_CMP_FLAG_IGNORE_SLASHES)) {
      b = str_slice(b, 1, b.len);
    }

    uint64_t len = a.len + sep.len + b.len;
    uint8_t *buf = ARENA_PUSH_ARRAY(arena, uint8_t, len);
    if (buf) {
      mem_copy(buf + 0,               a.data,   a.len);
      mem_copy(buf + a.len,           sep.data, sep.len);
      mem_copy(buf + a.len + sep.len, b.data,   b.len);
      result = str_make(buf, len);
    }
  }

  // normalize
  return path_replace_slashes_inplace(result, '/');
}
