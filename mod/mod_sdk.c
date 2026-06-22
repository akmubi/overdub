#include "mod_sdk.h"

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <windows.h>

#define QUEUE_PUSH(f, l, n) \
  ((f) == 0 ? (((f) = (l) = (n)), ((n)->next = 0)) : ((l)->next = (n), (l) = (n), ((n)->next = 0)))
#define QUEUE_PUSH_FRONT(f, l, n) \
  ((f) == 0 ? (((f) = (l) = (n)), ((n)->next = 0)) : ((n)->next = (f)), ((f) = (n)))
#define QUEUE_POP(f, l) \
  ((f) == (l) ? ((f) = 0, (l) = 0) : ((f) = (f)->next))

/* ======================================================= SDK  ===================================================== */

mod_sdk_t g_sdk = {0};

#define MOD_HOST_API_REQUIRED_SIZE (offsetof(mod_host_api_t, hook_remove) + sizeof(((mod_host_api_t *)0)->hook_remove))

static bool
mod_version_compatible(mod_version_t host, mod_version_t mod)
{
  return host.major == mod.major && host.minor == mod.minor;
}

bool
mod_sdk_init(const mod_host_api_t *host, mod_t mod)
{
  if (!host || mod == MOD_INVALID) {
    return false;
  }

  if (host->struct_size < MOD_HOST_API_REQUIRED_SIZE) {
    return false;
  }

  if (!mod_version_compatible(host->abi_version, (mod_version_t)MOD_ABI_VERSION)) {
    return false;
  }

  g_sdk.host = host;
  g_sdk.mod    = mod;
  return true;
}

const mod_host_api_t *
mod_sdk_host(void)
{
  MOD_ASSERT_MSG(g_sdk.host != NULL, "mod_sdk_init() must be called before using SDK functions");
  return g_sdk.host;
}

mod_t
mod_sdk_mod_handle(void)
{
  MOD_ASSERT_MSG(g_sdk.mod != MOD_INVALID, "mod_sdk_init() must be called before using SDK functions");
  return g_sdk.mod;
}

void
mod_assert_msg(const char *expr, const char *file, int line, const char *fmt, ...)
{
  char buf[4096];
  int  len = snprintf(buf, sizeof(buf),
                      "Expression: %s\n"
                      "Location:   %s:%d\n",
                      expr ? expr : "<none>",
                      file ? file : "<unknown>",
                      line);

  if (len < 0) {
    buf[0] = '\0';
    len    = 0;
  } else if ((size_t)len >= sizeof(buf)) {
    len = (int)sizeof(buf) - 1;
  }

  if (fmt && fmt[0] != '\0' && (size_t)len < sizeof(buf) - 1) {
    int written = snprintf(buf + len, sizeof(buf) - (size_t)len, "Message:    ");

    if (written > 0) {
      if ((size_t)written >= sizeof(buf) - (size_t)len) {
        len = (int)sizeof(buf) - 1;
      } else {
        len += written;

        va_list args;
        va_start(args, fmt);
        written = vsnprintf(buf + len, sizeof(buf) - (size_t)len, fmt, args);
        va_end(args);

        if (written < 0) {
          buf[len] = '\0';
        } else if ((size_t)written >= sizeof(buf) - (size_t)len) {
          len = (int)sizeof(buf) - 1;
        } else {
          len += written;
        }
      }
    }
  }

  if ((size_t)len < sizeof(buf) - 1) {
    buf[len++] = '\n';
    buf[len]   = '\0';
  }

  int result = MessageBoxA(NULL, buf,
                           "Assertion failed",
                           MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND | MB_TOPMOST);

  switch (result) {
    case IDABORT:
      abort();
      break;
    case IDRETRY:
      __debugbreak();
      break;
    case IDIGNORE:
    default:
      break;
  }
}

/* ======================================================= MOD  ===================================================== */

str_t
mod_get_mod_dir(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->get_mod_dir(mod) : STR_NULL;
}

str_t
mod_get_config_path(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->get_config_path(mod) : STR_NULL;
}

str_t
mod_get_manifest_path(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->get_manifest_path(mod) : STR_NULL;
}

void
mod_logv(log_level_t level, const char *fmt, va_list args)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  if (host && mod != MOD_INVALID) {
    host->logv(mod, level, fmt, args);
  }
}

void
mod_log(log_level_t level, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  mod_logv(level, fmt, args);
  va_end(args);
}

mod_cfg_t
mod_get_cfg_by_id(str_t id)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->get_cfg_by_id(mod, id) : MOD_CFG_INVALID;
}

bool
mod_register_cmd(str_t name, str_t description, mod_cmd_fn_t fn, void *user)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID)? host->register_cmd(mod, name, description, fn, user) : false;
}

/* ======================================================== UI ====================================================== */

struct nk_context *
mod_get_nk_ctx(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  return host ? host->get_nk_ctx() : NULL;
}

void
mod_get_viewport_size(unsigned int *vw, unsigned int *vh)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->get_viewport_size(vw, vh);
  }
}

/* ======================================================= CFG  ===================================================== */

bool
mod_cfg_get_bool(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  bool                  result = false;
  if (host) {
    host->cfg_get_bool(cfg, &result);
  }
  return result;
}

int
mod_cfg_get_int(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  int                   result = 0;
  if (host) {
    host->cfg_get_int(cfg, &result);
  }
  return result;
}

float
mod_cfg_get_float(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  float                 result = 0.0f;
  if (host) {
    host->cfg_get_float(cfg, &result);
  }
  return result;
}

int
mod_cfg_get_enum(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  int                   result = -1;
  if (host) {
    host->cfg_get_enum(cfg, &result);
  }
  return result;
}

uint64_t
mod_cfg_get_string_len(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  uint64_t              result = 0;
  if (host) {
    host->cfg_get_string_len(cfg, &result);
  }
  return result;
}

uint64_t
mod_cfg_get_string_data(mod_cfg_t cfg, void  *buf, uint64_t cap)
{
  const mod_host_api_t *host = mod_sdk_host();
  uint64_t              len  = 0;
  if (host && buf && cap > 0) {
    host->cfg_get_string_data(cfg, buf, cap, &len);
  }
  return len;
}

str_t
mod_cfg_push_string(mod_cfg_t cfg, mod_arena_t arena)
{
  mod_t mod    = mod_sdk_mod_handle();
  str_t result = STR_NULL;
  if (mod != MOD_INVALID) {
    uint64_t len = mod_cfg_get_string_len(cfg);
    if (len > 0) {
      uint8_t *data = MOD_ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, len + 1);
      if (data) {
        uint64_t nread = mod_cfg_get_string_data(cfg, data, len);
        result = str_make(data, nread);
      }
    }
  }
  return result;
}

keybind_t
mod_cfg_get_keybind(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  keybind_t             result = {0};
  if (host) {
    host->cfg_get_keybind(cfg, &result);
  }
  return result;
}

mod_color_t
mod_cfg_get_color(mod_cfg_t cfg)
{
  const mod_host_api_t *host   = mod_sdk_host();
  mod_color_t           result = {0};
  if (host) {
    host->cfg_get_color(cfg, &result);
  }
  return result;
}

void
mod_cfg_set_bool(mod_cfg_t cfg, bool val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_bool(cfg, val);
  }
}

void
mod_cfg_set_int(mod_cfg_t cfg, int val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_int(cfg, val);
  }
}

void
mod_cfg_set_float(mod_cfg_t cfg, float val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_float(cfg, val);
  }
}

void
mod_cfg_set_enum(mod_cfg_t cfg, int val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_enum(cfg, val);
  }
}

void
mod_cfg_set_string(mod_cfg_t cfg, str_t val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_string(cfg, val);
  }
}

void
mod_cfg_set_keybind(mod_cfg_t cfg, keybind_t val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_keybind(cfg, val);
  }
}

void
mod_cfg_set_color(mod_cfg_t cfg, mod_color_t val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->cfg_set_color(cfg, val);
  }
}

/* ====================================================== ARENA ===================================================== */

mod_arena_t
mod_get_perm(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->get_perm(mod) : MOD_ARENA_INVALID;
}

mod_arena_t
mod_arena_create(uint64_t reserve_size, uint64_t commit_size)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->arena_create(mod, reserve_size, commit_size) : MOD_ARENA_INVALID;
}

void
mod_arena_destroy(mod_arena_t arena)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  if (host && mod != MOD_INVALID) {
    host->arena_destroy(mod, arena);
  }
}

void *
mod_arena_push(mod_arena_t arena, uint64_t size, uint64_t alignment)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->arena_push(arena, size, alignment) : NULL;
}

void *
mod_arena_push_zero(mod_arena_t arena, uint64_t size, uint64_t alignment)
{
  void *data = mod_arena_push(arena, size, alignment);
  if (data) {
    memset(data, 0, size);
  }
  return data;
}

uint64_t
mod_arena_pos(mod_arena_t arena)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->arena_pos(arena) : 0ULL;
}

void
mod_arena_pop_to(mod_arena_t arena, uint64_t pos)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->arena_pop_to(arena, pos);
  }
}

void
mod_arena_pop(mod_arena_t arena, uint64_t size)
{
  uint64_t used = mod_arena_pos(arena);
  if (size > used) {
    size = used;
  }
  mod_arena_pop_to(arena, used - size);
}

void
mod_arena_reset(mod_arena_t arena)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->arena_reset(arena);
  }
}

tmp_arena_t
mod_scratch_begin(mod_arena_t conflict)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->scratch_begin(conflict) : (tmp_arena_t){0};
}

void
mod_scratch_end(tmp_arena_t tmp)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->scratch_end(tmp);
  }
}

/* ====================================================== INPUT ===================================================== */

input_key_kind_t
mod_input_key_from_str(str_t name)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->input_key_from_str(name) : INPUT_KEY_NONE;
}

str_t
mod_input_key_to_str(input_key_kind_t key)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->input_key_to_str(key) : STR_NULL;
}

str_t
mod_input_event_kind_to_str(input_event_kind_t kind)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->input_event_kind_to_str(kind) : STR_NULL;
}

bool
mod_keybind_parse(str_t str, keybind_t *out)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->keybind_parse(str, out) : false;
}

uint64_t
mod_keybind_utf8_len(keybind_t bind)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->keybind_utf8_len(bind) : 0ULL;
}

uint64_t
mod_keybind_utf8_write(keybind_t bind, void *buf, uint64_t cap)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host && buf && cap > 0) ? host->keybind_utf8_write(bind, buf, cap) : 0ULL;
}

str_t
mod_keybind_to_str(keybind_t bind, mod_arena_t arena)
{
  uint64_t len    = mod_keybind_utf8_len(bind);
  str_t    result = STR_NULL;
  if (len > 0) {
    uint8_t *data = MOD_ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, len + 1); // null-terminated just in case
    if (data) {
      uint64_t nread = mod_keybind_utf8_write(bind, data, len);
      data[nread] = '\0';

      result = str_make(data, nread);
    }
  }
  return result;
}

bool
mod_keybind_is_down(keybind_t bind)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->keybind_is_down(bind) : false;
}

bool
mod_keybind_was_down(keybind_t bind)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->keybind_was_down(bind) : false;
}

bool
mod_keybind_is_pressed(keybind_t bind)
{
  return mod_keybind_is_down(bind) && !mod_keybind_was_down(bind);
}

bool
mod_keybind_is_released(keybind_t bind)
{
  return !mod_keybind_is_down(bind) && mod_keybind_was_down(bind);
}

bool
mod_keybind_str_is_down(str_t keybind_str)
{
  keybind_t bind = {0};
  if (!mod_keybind_parse(keybind_str, &bind)) {
    return false;
  }
  return mod_keybind_is_down(bind);
}

bool
mod_keybind_str_was_down(str_t keybind_str)
{
  keybind_t bind = {0};
  if (!mod_keybind_parse(keybind_str, &bind)) {
    return false;
  }
  return mod_keybind_was_down(bind);
}

bool
mod_keybind_str_is_pressed(str_t keybind_str)
{
  keybind_t bind = {0};
  if (!mod_keybind_parse(keybind_str, &bind)) {
    return false;
  }
  return mod_keybind_is_pressed(bind);
}

bool
mod_keybind_str_is_released(str_t keybind_str)
{
  keybind_t bind = {0};
  if (!mod_keybind_parse(keybind_str, &bind)) {
    return false;
  }
  return mod_keybind_is_released(bind);
}

bool
mod_input_event_covers_keybind(input_event_t *ev, keybind_t bind)
{
  bool has_event_key = false;
  for (int i = 0; i < bind.count; ++i) {
    if (bind.keys[i] == ev->key) {
      has_event_key = true;
      break;
    }
  }
  return has_event_key;
}

/* ========================================== HOOKING / SIGNATURE SCANNING ========================================== */

sigscan_err_t
mod_sigscan(sigscan_entry_t *entry)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host && entry) ? host->sigscan(entry->name, entry->pattern, entry->op_off, entry->deref, entry->first_rvas, entry->pp) : SIGSCAN_ERR_NOT_FOUND;
}

bool
mod_hook_create(void *target, void *detour, void **original)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->hook_create(mod, target, detour, original) : false;
}

bool
mod_hook_enable(void *target)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->hook_enable(mod, target) : false;
}

bool
mod_hook_disable(void *target)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->hook_disable(mod, target) : false;
}

bool
mod_hook_remove(void *target)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host && mod != MOD_INVALID) ? host->hook_remove(mod, target) : false;
}

/* ===================================================== STRING ===================================================== */

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
str_push_copy(mod_arena_t arena, str_t str)
{
  str_t result = {
    .len  = str.len,
    .data = mod_arena_push(arena, str.len + 1, 1),
  };

  if (result.data) {
    if (result.len > 0) {
      memcpy(result.data, str.data, str.len);
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
    written = vsnprintf(NULL, 0, fmt, args_copy);
  } else {
    written = vsnprintf((char *)buf, (size_t)cap, fmt, args_copy);
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
str_push_vfmt(mod_arena_t arena, const char *fmt, va_list args)
{
  MOD_ASSERT(fmt != NULL);

  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(NULL, 0, fmt, args_copy);
  va_end(args_copy);

  if (needed < 0) {
    return (str_t){0};
  }

  uint64_t cap = (uint64_t)needed + 1;
  uint8_t *data = mod_arena_push(arena, cap, 1);
  if (!data) {
    return (str_t){0};
  }

  int written = vsnprintf((char *)data, (size_t)cap, fmt, args);
  if (written < 0 || written != needed) {
    return (str_t){0};
  }

  return (str_t){
    .data = data,
    .len  = (uint64_t)written,
  };
}

str_t
str_push_fmt(mod_arena_t arena, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  str_t result = str_push_vfmt(arena, fmt, args);
  va_end(args);
  return result;
}

str_t
str_push_fill_byte(mod_arena_t arena, uint64_t size, uint8_t byte)
{
  str_t result = {
    .data = mod_arena_push(arena, size + 1, 1),
    .len  = size,
  };

  if (result.data) {
    if (result.len > 0) {
      memset(result.data, byte, size + 1);
    }
    result.data[result.len] = '\0';
  }
  return result;
}

str_t
str_push_hex(mod_arena_t arena, void *data, uint64_t size)
{
  uint64_t hex_size = size * 2;
  str_t    result   = {
    .data = mod_arena_push(arena, hex_size + 1, 1),
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
str_push_concat(mod_arena_t arena, str_t a, str_t b)
{
  uint64_t len    = a.len + b.len;
  str_t    result = {
    .data = mod_arena_push(arena, len + 1, 1),
    .len  = len,
  };

  if (result.data) {
    if (result.len > 0) {
      memcpy(result.data + 0, a.data, a.len);
      memcpy(result.data + a.len, b.data, b.len);
    }
    result.data[result.len] = '\0';
  }

  return result;
}

str_list_t
str_list_copy(mod_arena_t arena, str_list_t src)
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
str_list_push(mod_arena_t arena, str_list_t *list, str_t str)
{
  str_node_t *node = mod_arena_push(arena, sizeof(str_node_t), sizeof(str_node_t));
  if (node) {
    node->str = str;
    str_list_push_node(list, node);
  }
}

void
str_list_push_copy(mod_arena_t arena, str_list_t *list, str_t str)
{
  str_list_push(arena, list, str_push_copy(arena, str));
}

void
str_list_push_fmt(mod_arena_t arena, str_list_t *list, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  str_t s = str_push_vfmt(arena, fmt, args);
  va_end(args);
  str_list_push(arena, list, s);
}

void
str_list_push_front(mod_arena_t arena, str_list_t *list, str_t str)
{
  str_node_t *node = mod_arena_push(arena, sizeof(str_node_t), sizeof(str_node_t));
  if (node) {
    node->str = str;
    str_list_push_node_front(list, node);
  }
}

void
str_list_push_front_fmt(mod_arena_t arena, str_list_t *list, const char *fmt, ...)
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
  memset(src, 0, sizeof(*src));
}

str_list_t
str_split(mod_arena_t arena, str_t str, uint64_t split_count, str_t *splits)
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
str_split_lines(mod_arena_t arena, str_t str)
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
str_list_join(mod_arena_t arena, str_list_t list, str_t pre, str_t sep, str_t post)
{
  uint64_t seps  = (list.count > 1) ? (list.count - 1) : 0;
  uint64_t total = (list.total_bytes + pre.len + seps * sep.len + post.len);

  str_t result = {
    .data = mod_arena_push(arena, total + 1, 1),
    .len  = total,
  };

  if (result.data) {
    if (result.len > 0) {
      uint8_t *ptr = result.data;
      memcpy(ptr, pre.data, pre.len);
      ptr += pre.len;

      for (str_node_t *node = list.first; node; node = node->next) {
        memcpy(ptr, node->str.data, node->str.len);
        ptr += node->str.len;

        if (node != list.last) {
          memcpy(ptr, sep.data, sep.len);
          ptr += sep.len;
        }
      }

      memcpy(ptr, post.data, post.len);
      ptr += post.len;
    }

    result.data[result.len] = '\0';
  }
  return result;
}

str_t
str_list_join_lines(mod_arena_t arena, str_list_t list)
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
str_array_copy(mod_arena_t arena, str_array_t src)
{
  MOD_ASSERT((!src.items && src.count == 0) || (src.items && src.count > 0));

  str_array_t dst = {
    .count = src.count,
    .items = mod_arena_push(arena, src.count * sizeof(str_t), sizeof(str_t)),
  };

  if (dst.items) {
    for (uint64_t i = 0; i < src.count; ++i) {
      dst.items[i] = str_push_copy(arena, src.items[i]);
    }
  }
  return dst;
}

str_array_t
str_array_copy_from_list(mod_arena_t arena, str_list_t list)
{
  return str_array_from_list(arena, str_list_copy(arena, list));
}

str_array_t
str_array_from_list(mod_arena_t arena, str_list_t list)
{
  str_array_t array = {
    .count = list.count,
    .items = mod_arena_push(arena, list.count * sizeof(str_t), sizeof(str_t)),
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
str_push_cstr(mod_arena_t arena, str_t s)
{
  char *cstr = mod_arena_push(arena, s.len + 1, 1);
  if (cstr) {
    memcpy(cstr, s.data, s.len * sizeof(char));
    cstr[s.len] = '\0';
  }
  return cstr;
}

wchar_t *
str16_push_wstr(mod_arena_t arena, str16_t s)
{
  wchar_t *wstr = mod_arena_push(arena, sizeof(s.data[0]) * (s.len + 1), sizeof(s.data[0]));
  if (wstr) {
    memcpy(wstr, s.data, s.len * sizeof(wchar_t));
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
  memcpy(buf, s.data, len - 1);
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
  memcpy(buf, s.data, len);
  return len;
}

str_t
str_to_upper(mod_arena_t arena, str_t str)
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
str_to_lower(mod_arena_t arena, str_t str)
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

#define BITMASK1  0x01
#define BITMASK2  0x03
#define BITMASK3  0x07
#define BITMASK4  0x0F
#define BITMASK5  0x1F
#define BITMASK6  0x3F
#define BITMASK7  0x7F
#define BITMASK8  0xFF
#define BITMASK9  0x01FF
#define BITMASK10 0x03FF

typedef struct decoded_codepoint_s decoded_codepoint_t;
struct decoded_codepoint_s {
  uint32_t codepoint;
  uint32_t advance;
};

static uint8_t utf8_class[32] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 5,
};

static decoded_codepoint_t
codepoint_from_utf8(uint8_t *str, uint64_t max)
{
  decoded_codepoint_t result = {
    .codepoint = 0xFFFFFFFF,
    .advance   = 1,
  };

  uint8_t byte       = str[0];
  uint8_t byte_class = utf8_class[byte >> 3];

  switch (byte_class) {
    case 1:
      result.codepoint = byte;
      break;

    case 2:
      if (2 <= max) {
        uint8_t cont_byte = str[1];
        if (utf8_class[cont_byte >> 3] == 0) {
          result.codepoint  = (byte & BITMASK5) << 6;
          result.codepoint |= (cont_byte & BITMASK6);
          result.advance    = 2;
        }
      }
      break;

    case 3:
      if (3 <= max) {
        uint8_t cont_byte[2] = {str[1], str[2]};
        if (utf8_class[cont_byte[0] >> 3] == 0 && utf8_class[cont_byte[1] >> 3] == 0) {
          result.codepoint  = (byte & BITMASK4) << 12;
          result.codepoint |= ((cont_byte[0] & BITMASK6) << 6);
          result.codepoint |= (cont_byte[1] & BITMASK6);
          result.advance    = 3;
        }
      }
      break;

    case 4:
      if (4 <= max) {
        uint8_t cont_byte[3] = {str[1], str[2], str[3]};
        if (utf8_class[cont_byte[0] >> 3] == 0 && utf8_class[cont_byte[1] >> 3] == 0 &&
            utf8_class[cont_byte[2] >> 3] == 0) {
          result.codepoint  = (byte & BITMASK3) << 18;
          result.codepoint |= ((cont_byte[0] & BITMASK6) << 12);
          result.codepoint |= ((cont_byte[1] & BITMASK6) << 6);
          result.codepoint |= (cont_byte[2] & BITMASK6);
          result.advance    = 4;
        }
      }
      break;
  }

  if (result.advance == 2 && result.codepoint < 0x80) {
    result.codepoint = 0xFFFFFFFF;
  }

  if (result.advance == 3 && result.codepoint < 0x800) {
    result.codepoint = 0xFFFFFFFF;
  }

  if (result.advance == 4 && (result.codepoint < 0x10000 || result.codepoint > 0x10FFFF)) {
    result.codepoint = 0xFFFFFFFF;
  }

  if (result.codepoint >= 0xD800 && result.codepoint <= 0xDFFF) {
    result.codepoint = 0xFFFFFFFF;
  }

  return result;
}

static decoded_codepoint_t
codepoint_from_utf16(uint16_t *str, uint64_t max)
{
  decoded_codepoint_t result = {
    .codepoint = 0xFFFFFFFF,
    .advance   = 1,
  };

  if (!max) {
    return result;
  }

  uint16_t w0 = str[0];
  if (w0 >= 0xD800 && w0 <= 0xDBFF) {
    if (max >= 2) {
      uint16_t w1 = str[1];
      if (w1 >= 0xDC00 && w1 <= 0xDFFF) {
        result.codepoint = 0x10000 + (((uint32_t)(w0 - 0xD800) << 10) | ((uint32_t)(w1 - 0xDC00) & 0x3FF));
        result.advance   = 2;
        return result;
      }
    }
    return result;
  }

  if (w0 >= 0xDC00 && w0 <= 0xDFFF) {
    return result;
  }

  result.codepoint = w0;
  return result;
}

static uint64_t
utf8_len_from_codepoint(uint32_t codepoint)
{
#define BIT8 0x80
  uint32_t advance = 0;
  if (codepoint <= 0x7F) {
    advance = 1;
  } else if (codepoint <= 0x7FF) {
    advance = 2;
  } else if (codepoint <= 0xFFFF) {
    advance = 3;
  } else if (codepoint <= 0x10FFFF) {
    advance = 4;
  } else {
    advance = 1;
  }
#undef BIT8
  return advance;
}

static uint32_t
utf8_from_codepoint(uint8_t *out, uint32_t codepoint)
{
#define BIT8 0x80
  uint32_t advance = 0;
  if (codepoint <= 0x7F) {
    out[0]  = (uint8_t)codepoint;
    advance = 1;
  } else if (codepoint <= 0x7FF) {
    out[0]  = (BITMASK2 << 6) | ((codepoint >> 6) & BITMASK5);
    out[1]  = BIT8 | (codepoint & BITMASK6);
    advance = 2;
  } else if (codepoint <= 0xFFFF) {
    out[0]  = (BITMASK3 << 5) | ((codepoint >> 12) & BITMASK4);
    out[1]  = BIT8 | ((codepoint >> 6) & BITMASK6);
    out[2]  = BIT8 | (codepoint & BITMASK6);
    advance = 3;
  } else if (codepoint <= 0x10FFFF) {
    out[0]  = (BITMASK4 << 4) | ((codepoint >> 18) & BITMASK3);
    out[1]  = BIT8 | ((codepoint >> 12) & BITMASK6);
    out[2]  = BIT8 | ((codepoint >> 6) & BITMASK6);
    out[3]  = BIT8 | (codepoint & BITMASK6);
    advance = 4;
  } else {
    out[0]  = '?';
    advance = 1;
  }
#undef BIT8
  return advance;
}

static uint32_t
utf16_from_codepoint(uint16_t *out, uint32_t codepoint)
{
  uint32_t advance = 1;
  if (codepoint == 0xFFFFFFFF) {
    out[0] = (uint16_t)'?';
  } else if (codepoint < 0x10000) {
    out[0] = (uint16_t)codepoint;
  } else {
    uint64_t v = codepoint - 0x10000;
    out[0]  = 0xD800 + (uint16_t)(v >> 10);
    out[1]  = 0xDC00 + (uint16_t)(v & BITMASK10);
    advance = 2;
  }
  return advance;
}

#undef BITMASK10
#undef BITMASK9
#undef BITMASK8
#undef BITMASK7
#undef BITMASK6
#undef BITMASK5
#undef BITMASK4
#undef BITMASK3
#undef BITMASK2
#undef BITMASK1

str_t
str_from_str16(mod_arena_t arena, str16_t in)
{
  uint64_t  cap  = in.len * 3; // worst case UTF-8
  uint8_t  *buf  = mod_arena_push(arena, cap + 1, 1);
  uint16_t *ptr  = in.data;
  uint16_t *opl  = ptr + in.len;
  uint64_t  size = 0;

  if (buf) {
    while (ptr < opl) {
      decoded_codepoint_t c = codepoint_from_utf16(ptr, (uint64_t)(opl - ptr));

      ptr  += c.advance;
      size += utf8_from_codepoint(buf + size, c.codepoint);
    }
    buf[size] = 0;
    mod_arena_pop(arena, cap - size);
  }

  return str_make(buf, size);
}

str16_t
str16_from_str(mod_arena_t arena, str_t in)
{
  uint64_t  cap  = in.len * 2; // worst case UTF-16
  uint16_t *buf  = mod_arena_push(arena, (cap + 1) * 2, 2);
  uint8_t  *ptr  = in.data;
  uint8_t  *opl  = ptr + in.len;
  uint64_t  size = 0;

  if (buf) {
    while (ptr < opl) {
      decoded_codepoint_t c = codepoint_from_utf8(ptr, (uint64_t)(opl - ptr));

      ptr  += c.advance;
      size += utf16_from_codepoint(buf + size, c.codepoint);
    }
    buf[size] = 0;
    mod_arena_pop(arena, (cap - size) * sizeof(uint16_t));
  }

  return str16_make(buf, size);
}

uint64_t
str16_utf8_len(str16_t in)
{
  uint16_t *ptr = in.data;
  uint16_t *opl = ptr + in.len;
  uint64_t  len = 0;

  while (ptr < opl) {
    decoded_codepoint_t c = codepoint_from_utf16(ptr, (uint64_t)(opl - ptr));

    ptr += c.advance;
    len += utf8_len_from_codepoint(c.codepoint);
  }

  return len;
}

uint64_t
str16_write_utf8(uint8_t *buf, uint64_t max_len, str16_t in)
{
  uint16_t *ptr = in.data;
  uint16_t *opl = ptr + in.len;
  uint64_t  len = 0;

  while (ptr < opl) {
    decoded_codepoint_t c = codepoint_from_utf16(ptr, (uint64_t)(opl - ptr));
    uint64_t bytes = utf8_len_from_codepoint(c.codepoint);

    if (len + bytes > max_len) {
      break;
    }

    ptr += c.advance;
    len += utf8_from_codepoint(buf + len, c.codepoint);
  }

  return len;
}

/* ====================================================== PATH ====================================================== */

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
path_push_relative(mod_arena_t arena, str_t path, str_t base)
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
path_push_parts(mod_arena_t arena, str_t path)
{
  str_t splits[] = {
    STR_LIT("/"),
    STR_LIT("\\"),
  };

  return str_split(arena, path, COUNTOF(splits), splits);
}

str_t
path_module_dir(mod_arena_t perm)
{
  wchar_t buf[MAX_PATH + 1] = {0};
  HMODULE h_mod             = NULL;

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
path_join(mod_arena_t arena, str_t a, str_t b)
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
    uint8_t *buf = mod_arena_push(arena, len, 1);
    if (buf) {
      memcpy(buf + 0,               a.data,   a.len);
      memcpy(buf + a.len,           sep.data, sep.len);
      memcpy(buf + a.len + sep.len, b.data,   b.len);
      result = str_make(buf, len);
    }
  }

  // normalize
  return path_replace_slashes_inplace(result, '/');
}

/* ====================================================== FILE ====================================================== */

bool
file_exists(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  DWORD attr   = GetFileAttributesW(path16.data);
  bool  exists = (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);

  mod_scratch_end(tmp);
  return exists;
}

uint64_t
file_size(str_t path)
{
  if (str_is_empty(path)) {
    return 0;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);
  uint64_t    size   = 0;

  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExW(path16.data, GetFileExInfoStandard, &data)) {
    size = ((uint64_t)data.nFileSizeHigh << 32) | (uint64_t)data.nFileSizeLow;
  }

  mod_scratch_end(tmp);
  return size;
}

str_t
file_read_all(str_t path, mod_arena_t arena)
{
  str_t result = {0};
  if (str_is_empty(path) || !arena) {
    return result;
  }

  tmp_arena_t tmp    = mod_scratch_begin(arena);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  HANDLE h = CreateFileW(path16.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER size_li;
    if (GetFileSizeEx(h, &size_li) && size_li.QuadPart > 0) {
      uint64_t size = size_li.QuadPart;
      uint64_t used = mod_arena_pos(arena);
      uint8_t *data = MOD_ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, size + 1);
      if (data) {
        uint64_t total_read = 0;
        while (total_read < size) {
          DWORD to_read   = (DWORD)MIN_VAL((uint64_t)UINT32_MAX, (size - total_read));
          DWORD just_read = 0;

          if (!ReadFile(h, data + total_read, to_read, &just_read, NULL)) {
            break;
          }

          if (just_read == 0) {
            break;
          }

          total_read += just_read;
        }

        if (total_read != size) {
          mod_arena_pop_to(arena, used);
        } else {
          result = str_make(data, total_read);
        }
      }
    }
    CloseHandle(h);
  }

  mod_scratch_end(tmp);
  return result;
}

bool
file_write(str_t s, str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);
  bool        result = false;

  HANDLE h = CreateFileW(path16.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    if (str_is_empty(s)) {
      // empty string -> empty file
      result = true;
    } else {
      bool     success       = true;
      uint64_t total_written = 0;
      while (total_written < s.len) {
        DWORD to_write     = (DWORD)MIN_VAL((uint64_t)UINT32_MAX, (s.len - total_written));
        DWORD just_written = 0;

        if (!WriteFile(h, s.data + total_written, to_write, &just_written, NULL)) {
          success = false;
          break;
        }

        if (just_written == 0) {
          success = false;
          break;
        }

        total_written += (uint64_t)just_written;
      }
      result = success && (total_written == s.len);
    }
    CloseHandle(h);
  }

  mod_scratch_end(tmp);
  return result;
}

bool
file_write_lines(str_list_t lines, str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);
  bool        result = false;

  HANDLE h = CreateFileW(path16.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    bool     success       = true;
    uint64_t total_written = 0;
    uint64_t total_size    = lines.total_bytes + (lines.count > 1 ? lines.count - 1 : 0);

    for (str_node_t *node = lines.first; node && success; node = node->next) {
      str_t chunks[] = {
        node->str,
        (node != lines.last) ? STR_LIT("\n") : STR_NULL,
      };

      for (uint64_t chunk_idx = 0; chunk_idx < 2; ++chunk_idx) {
        str_t chunk = chunks[chunk_idx];
        if (str_is_empty(chunk)) {
          continue;
        }

        uint64_t chunk_written = 0;
        while (chunk_written < chunk.len) {
          DWORD to_write     = (DWORD)MIN_VAL((uint64_t)UINT32_MAX, (chunk.len - chunk_written));
          DWORD just_written = 0;

          if (!WriteFile(h, chunk.data + chunk_written, to_write, &just_written, NULL)) {
            success = false;
            break;
          }

          if (just_written == 0) {
            success = false;
            break;
          }

          chunk_written += (uint64_t)just_written;
          total_written += (uint64_t)just_written;
        }
      }
    }

    result = success && (total_written == total_size);
    CloseHandle(h);
  }

  mod_scratch_end(tmp);
  return result;
}

bool
file_copy(str_t dst, str_t src)
{
  if (str_is_empty(dst) || str_is_empty(src)) {
    return false;
  }

  tmp_arena_t tmp   = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     src16 = str16_from_str(tmp.arena, src);
  str16_t     dst16 = str16_from_str(tmp.arena, dst);

  bool result = (CopyFileW(src16.data, dst16.data, FALSE) != 0);

  mod_scratch_end(tmp);
  return result;
}

bool
file_delete(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  bool result = (DeleteFileW(path16.data) != 0);

  mod_scratch_end(tmp);
  return result;
}

bool
dir_exists(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  DWORD attr   = GetFileAttributesW(path16.data);
  bool  exists = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);

  mod_scratch_end(tmp);
  return exists;
}

bool
dir_create(str_t path)
{
  if (dir_exists(path)) {
    return true;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  bool result = CreateDirectoryW(path16.data, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;

  mod_scratch_end(tmp);
  return result;
}

bool
dir_create_recursive(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  if (dir_exists(path)) {
    return true;
  }

  /* walk the path and create each segment */
  for (uint64_t i = 0; i < path.len; ++i) {
    uint8_t c = path.data[i];
    if (c == '/' || c == '\\') {
      str_t segment = str_slice(path, 0, i);
      if (!str_is_empty(segment) && !dir_exists(segment)) {
        if (!dir_create(segment)) {
          return false;
        }
      }
    }
  }

  return dir_create(path); 
}

static inline bool
is_current_or_parent_dir(const wchar_t *name)
{
  if (!name || name[0] == L'\0') {
    return false;
  }

  /* '.' and '..' */
  return (name[0] == L'.' && (name[1] == L'\0' || (name[1] == L'.' && name[2] == L'\0')));
}

static dir_entry_list_t
dir_list_internal(str_t path, str_t pattern, mod_arena_t arena)
{
  tmp_arena_t      tmp    = mod_scratch_begin(arena);
  dir_entry_list_t result = {0};

  path = path_replace_slashes_inplace(str_push_copy(tmp.arena, path), '\\');

  /* build search pattern: path\* or path\pattern */
  str_t search_pattern;
  if (str_is_empty(path)) {
    if (str_is_empty(pattern)) {
      search_pattern = STR_LIT("*");
    } else {
      search_pattern = pattern;
    }
  } else {
    if (str_is_empty(pattern)) {
      search_pattern = str_push_fmt(tmp.arena, "%.*s\\*", STR_ARG(path));
    } else {
      search_pattern = str_push_fmt(tmp.arena, "%.*s\\%.*s", STR_ARG(path), STR_ARG(pattern));
    }
  }

  str16_t pattern16 = str16_from_str(tmp.arena, search_pattern);

  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(pattern16.data, &fd);
  if (h != INVALID_HANDLE_VALUE) {
    /* first pass: count entries */
    int count = 0;
    do {
      if (is_current_or_parent_dir(fd.cFileName)) {
        continue;
      }
      count += 1;
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    if (count > 0) {
      /* second pass: fill entries */
      result.items = MOD_ARENA_PUSH_ARRAY_ZERO(arena, dir_entry_t, count);
      result.count = 0;

      if (result.items) {
        h = FindFirstFileW(pattern16.data, &fd);
        if (h != INVALID_HANDLE_VALUE) {
          do {
            if (is_current_or_parent_dir(fd.cFileName)) {
              continue;
            }

            if (result.count >= count) {
              break;
            }

            dir_entry_t *entry  = &result.items[result.count];
            str16_t      name16 = str16_from_wstr(fd.cFileName);

            entry->name = str_from_str16(arena, name16);
            entry->path = path_join(arena, path, entry->name);
            entry->kind = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DIR_ENTRY_DIR : DIR_ENTRY_FILE;
            entry->size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;

            result.count += 1;
          } while (FindNextFileW(h, &fd));
          FindClose(h);
        }
      }
    }
  }

  mod_scratch_end(tmp);
  return result;
}

dir_entry_list_t
dir_list(str_t path, mod_arena_t arena)
{
  return dir_list_internal(path, (str_t){0}, arena);
}

dir_entry_list_t
dir_list_filter(str_t path, str_t pattern, mod_arena_t arena)
{
  return dir_list_internal(path, pattern, arena);
}

bool
dir_delete(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  bool result = (RemoveDirectoryW(path16.data) != 0);

  mod_scratch_end(tmp);
  return result;
}

bool
dir_delete_recursive(str_t path)
{
  tmp_arena_t      tmp     = mod_scratch_begin(MOD_ARENA_INVALID);
  dir_entry_list_t entries = dir_list(path, tmp.arena);
  for (int i = 0; i < entries.count; ++i) {
    dir_entry_t *e = &entries.items[i];
    if (e->kind == DIR_ENTRY_DIR) {
      dir_delete_recursive(e->path);
    } else {
      file_delete(e->path);
    }
  }

  mod_scratch_end(tmp);
  return dir_delete(path);
}

/* =================================================== INI PARSER =================================================== */

bool
ini_is_line_section(str_t line)
{
  return (line.len >= 2 && line.data[0] == '[' && line.data[line.len - 1] == ']');
}

bool
ini_parse_section_header(str_t line, str_t *out_name, str_t *out_arg)
{
  *out_name = (str_t){0};
  *out_arg  = (str_t){0};

  str_t    body    = str_trim_space(str_slice(line, 1, line.len - 1));
  uint64_t dot_idx = 0;
  if (str_find(body, STR_LIT("."), 0, &dot_idx)) {
    *out_name = str_trim_space(str_slice(body, 0, dot_idx));
    *out_arg  = str_trim_space(str_slice(body, dot_idx + 1, body.len));
  } else {
    *out_name = body;
  }

  return !str_is_empty(*out_name);
}

static void
ini_section_list_push(ini_section_list_t *list, ini_section_t *section)
{
  QUEUE_PUSH(list->first, list->last, section);
  list->count += 1;
}

ini_section_list_t
ini_parse_sections(mod_arena_t arena, str_array_t lines)
{
  ini_section_list_t result  = {0};
  ini_section_t     *section = NULL;
  for (uint64_t i = 0; i < lines.count; ++i) {
    str_t line = lines.items[i];

    if (ini_is_line_section(line)) {
      str_t name = {0};
      str_t arg  = {0};
      if (ini_parse_section_header(line, &name, &arg)) {
        section = MOD_ARENA_PUSH_ZERO(arena, ini_section_t);
        if (section) {
          section->name = str_push_copy(arena, name);
          section->arg  = str_push_copy(arena, arg);

          ini_section_list_push(&result, section);
        }
      } else {
        section = NULL;
      }
      continue;
    }

    if (section) {
      str_list_push(arena, &section->lines, str_push_copy(arena, line));
    }
  }

  return result;
}

bool
ini_parse_kv(str_t line, str_t *out_key, str_t *out_value)
{
  if (!out_key || !out_value) {
    return false;
  }

  if (str_is_empty(line)) {
    return false;
  }

  *out_key   = (str_t){0};
  *out_value = (str_t){0};

  uint64_t eq_idx = 0;
  if (!str_find(line, STR_LIT("="), 0, &eq_idx)) {
    return false;
  }

  *out_key   = str_trim_space(str_slice(line, 0, eq_idx));
  *out_value = str_trim_space(str_slice(line, eq_idx + 1, line.len));

  return !str_is_empty(*out_key);
}

str_array_t
ini_split_lines(mod_arena_t arena, str_t text)
{
  if (!arena || str_is_empty(text)) {
    return (str_array_t){0};
  }

  tmp_arena_t tmp       = mod_scratch_begin(arena);
  str_array_t out_array = {0};
  {
    str_list_t  parts     = {0};
    str_list_t  raw_lines = str_split_lines(tmp.arena, text);
    for (str_node_t *node = raw_lines.first; node; node = node->next) {
      str_t line = node->str;

      line = str_trim_comment(line, STR_LIT(";"), 0);
      line = str_trim_space(line);

      str_list_push(tmp.arena, &parts, line);
    }
    out_array = str_array_copy_from_list(arena, parts);
  }
  mod_scratch_end(tmp);

  return out_array;
}