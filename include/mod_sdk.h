#ifndef MOD_SDK_H
#define MOD_SDK_H

#include "mod_types.h"
#include "sigscan.h"

typedef struct mod_host_api_s mod_host_api_t;
struct mod_host_api_s {
  mod_version_t abi_version;

  /* MOD */

  void     (MOD_CALL *log)     (mod_handle_t h, log_level_t level, const char *fmt, ...);
  void     (MOD_CALL *logv)    (mod_handle_t h, log_level_t level, const char *fmt, va_list args);
  void     (MOD_CALL *set_user)(mod_handle_t h, void *user);
  void    *(MOD_CALL *get_user)(mod_handle_t h);
  arena_t *(MOD_CALL *get_perm)(mod_handle_t h);

  /* ARENA */

  arena_t *(MOD_CALL *arena_new_dynamic)        (mod_handle_t h, uint64_t reserve, uint64_t commit);
  arena_t *(MOD_CALL *arena_new_static_borrowed)(mod_handle_t h, void *buf, uint64_t size);
  void     (MOD_CALL *arena_destroy)            (arena_t *a);
  void    *(MOD_CALL *arena_push_aligned)       (arena_t *a, uint64_t size, uint64_t align);
  void     (MOD_CALL *arena_pop_to)             (arena_t *a, uint64_t pos);
  uint64_t (MOD_CALL *arena_pos)                (arena_t *a);
  arena_t *(MOD_CALL *scratch_get)              (arena_t *conflict);

  /* STRING */

  uint64_t    (MOD_CALL *calc_cstr_len)           (const char *cstr);
  uint64_t    (MOD_CALL *calc_cstr_len_with_cap)  (const char *str, uint64_t cap);
  uint64_t    (MOD_CALL *calc_wstr_len)           (const wchar_t *wstr);
  uint64_t    (MOD_CALL *calc_wstr_len_with_cap)  (const wchar_t *str, uint64_t cap);

  str_t       (MOD_CALL *str_make)                (uint8_t *data, uint64_t size);
  str_t       (MOD_CALL *str_from_cstr)           (const char *cstr);
  str_t       (MOD_CALL *str_from_cstr_with_cap)  (const char *cstr, uint64_t cap);
  str16_t     (MOD_CALL *str16_make)              (uint16_t *data, uint64_t size);
  str16_t     (MOD_CALL *str16_from_wstr)         (const wchar_t *wstr);
  str16_t     (MOD_CALL *str16_from_wstr_with_cap)(const wchar_t *wstr, uint64_t cap);
  str_t       (MOD_CALL *str_slice)               (str_t str, uint64_t start_idx, uint64_t end_idx);

  bool        (MOD_CALL *str_has_prefix)          (str_t s, str_t prefix, str_cmp_flags_t flags);
  bool        (MOD_CALL *str_has_suffix)          (str_t s, str_t suffix, str_cmp_flags_t flags);
  int         (MOD_CALL *str_compare)             (str_t a, str_t b, str_cmp_flags_t flags);
  bool        (MOD_CALL *str_equal)               (str_t a, str_t b, str_cmp_flags_t flags);
  bool        (MOD_CALL *str_find)                (str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx);
  bool        (MOD_CALL *str_rfind)               (str_t s, str_t sub, str_cmp_flags_t flags, uint64_t *idx);

  str_t       (MOD_CALL *str_push_copy)           (arena_t *arena, str_t str);
  str_t       (MOD_CALL *str_push_vfmt)           (arena_t *arena, const char *fmt, va_list args);
  str_t       (MOD_CALL *str_push_fmt)            (arena_t *arena, const char *fmt, ...);
  str_t       (MOD_CALL *str_push_concat)         (arena_t *arena, str_t a, str_t b);
  str_t       (MOD_CALL *str_push_hex)            (arena_t *arena, void *data, uint64_t size);

  str_list_t  (MOD_CALL *str_list_copy)           (arena_t *arena, str_list_t src);
  void        (MOD_CALL *str_list_push)           (arena_t *arena, str_list_t *list, str_t str);
  void        (MOD_CALL *str_list_push_copy)      (arena_t *arena, str_list_t *list, str_t str);
  void        (MOD_CALL *str_list_push_fmt)       (arena_t *arena, str_list_t *list, const char *fmt, ...);
  void        (MOD_CALL *str_list_concat_inplace) (str_list_t *dst, str_list_t *src);
  bool        (MOD_CALL *str_list_contains)       (str_list_t list, str_t s, str_cmp_flags_t flags);
  str_list_t  (MOD_CALL *str_split)               (arena_t *arena, str_t str, uint64_t split_count, str_t *splits);
  str_t       (MOD_CALL *str_list_join)           (arena_t *arena, str_list_t list, str_t pre, str_t sep, str_t post);
  str_array_t (MOD_CALL *str_array_copy)          (arena_t *arena, str_array_t a);
  str_array_t (MOD_CALL *str_array_copy_from_list)(arena_t *arena, str_list_t list);
  str_array_t (MOD_CALL *str_array_from_list)     (arena_t *arena, str_list_t list);
  bool        (MOD_CALL *str_array_find)          (str_array_t array, str_t s, str_cmp_flags_t flags, uint64_t *idx);

  str_t       (MOD_CALL *str_from_str16)          (arena_t *arena, str16_t in);
  str16_t     (MOD_CALL *str16_from_str)          (arena_t *arena, str_t in);

  char       *(MOD_CALL *str_push_cstr)           (arena_t *arena, str_t s);
  wchar_t    *(MOD_CALL *str16_push_wstr)         (arena_t *arena, str16_t s);

  str_t       (MOD_CALL *str_trim_leading)        (str_t str, str_t charset, str_cmp_flags_t flags);
  str_t       (MOD_CALL *str_trim_trailing)       (str_t str, str_t charset, str_cmp_flags_t flags);
  str_t       (MOD_CALL *str_trim)                (str_t str, str_t charset, str_cmp_flags_t flags);
  str_t       (MOD_CALL *str_trim_prefix)         (str_t str, str_t prefix, str_cmp_flags_t flags);
  str_t       (MOD_CALL *str_trim_suffix)         (str_t str, str_t suffix, str_cmp_flags_t flags);

  bool        (MOD_CALL *str_parse_s64)           (str_t str, int64_t *value);
  bool        (MOD_CALL *str_parse_s32)           (str_t str, int32_t *value);
  bool        (MOD_CALL *str_parse_u32)           (str_t str, uint32_t *value);
  bool        (MOD_CALL *str_parse_u16)           (str_t str, uint16_t *value);
  bool        (MOD_CALL *str_parse_u8)            (str_t str, uint8_t *value);
  bool        (MOD_CALL *str_parse_float)         (str_t str, float *value);
  bool        (MOD_CALL *str_parse_bool)          (str_t str, bool default_val);

  /* FILE */

  bool             (MOD_CALL *file_exists)         (str_t path);
  uint64_t         (MOD_CALL *file_size)           (str_t path);
  str_t            (MOD_CALL *file_read_all)       (str_t path, arena_t *arena);
  bool             (MOD_CALL *file_write)          (str_t data, str_t path);
  bool             (MOD_CALL *dir_exists)          (str_t path);
  bool             (MOD_CALL *dir_create)          (str_t path);
  bool             (MOD_CALL *dir_create_recursive)(str_t path);
  dir_entry_list_t (MOD_CALL *dir_list)            (str_t path, arena_t *arena);
  dir_entry_list_t (MOD_CALL *dir_list_filter)     (str_t path, str_t pattern, arena_t *arena);

  /* PATH */

  str_t (MOD_CALL *path_base)      (str_t path);
  str_t (MOD_CALL *path_dir)       (str_t path);
  str_t (MOD_CALL *path_get_ext)   (str_t path);
  str_t (MOD_CALL *path_trim_ext)  (str_t path);
  bool  (MOD_CALL *path_is_abs)    (str_t path);
  str_t (MOD_CALL *path_module_dir)(arena_t *perm);
  str_t (MOD_CALL *path_join)      (arena_t *arena, str_t a, str_t b);

  /* HOOKING */

  bool (MOD_CALL *hook_create) (void *target, void *detour, void **original);
  bool (MOD_CALL *hook_enable) (void *target);
  bool (MOD_CALL *hook_disable)(void *target);
  bool (MOD_CALL *hook_remove) (void *target);

  /* SIGSCAN */

  sig_err_t (MOD_CALL *sigscan_scan_entry)(sig_entry_t *e);

  /* INPUT */

  input_key_kind_t (MOD_CALL *input_key_from_fname)    (fname_t key_name);
  input_key_kind_t (MOD_CALL *input_key_from_str)      (str_t name);
  str_t            (MOD_CALL *input_key_to_str)        (input_key_kind_t key);
  str_t            (MOD_CALL *input_event_kind_to_str) (input_event_kind_t kind);
  bool             (MOD_CALL *keybind_parse)           (str_t str, keybind_t *out);
  str_t            (MOD_CALL *keybind_to_str)          (keybind_t *bind, arena_t *arena);
  bool             (MOD_CALL *keybind_is_down)         (keybind_t *bind);
  bool             (MOD_CALL *keybind_was_down)        (keybind_t *bind);
  bool             (MOD_CALL *keybind_is_pressed)      (keybind_t *bind);
  bool             (MOD_CALL *keybind_is_released)     (keybind_t *bind);
  bool             (MOD_CALL *event_keybind_is_pressed)(input_event_t *ev, keybind_t *bind);

  /* UNREAL ENGINE API */

  bool       (MOD_CALL *fname_equal)                     (fname_t a, fname_t b);
  str_t      (MOD_CALL *fname_to_str)                    (fname_t fname, arena_t *perm);
  fname_t    (MOD_CALL *fname_from_str)                  (str_t s);
  bool       (MOD_CALL *uobject_is_a)                    (uobject_t *obj, uclass_t *cls);
  bool       (MOD_CALL *uobject_is_default)              (uobject_t *obj);
  bool       (MOD_CALL *uobject_is_valid)                (uobject_t *obj);
  str_t      (MOD_CALL *uobject_get_name)                (uobject_t *obj, arena_t *arena);
  str_t      (MOD_CALL *uobject_get_full_name)           (uobject_t *obj, arena_t *arena);
  str_t      (MOD_CALL *uobject_get_full_name_with_class)(uobject_t *obj, arena_t *arena);
  int        (MOD_CALL *uobject_array_count)             (void);
  uobject_t *(MOD_CALL *uobject_find)                    (uclass_t *cls, str_t name);
  uobject_t *(MOD_CALL *uobject_find_by_full_name)       (uclass_t *cls, str_t full_name);
  uobject_t *(MOD_CALL *uobject_find_first_of)           (uclass_t *cls);
  uclass_t  *(MOD_CALL *uobject_find_class_by_full_name) (str_t full_name);
  uobject_t *(MOD_CALL *uobject_spawn_actor)             (uobject_t *world_ctx_obj, uclass_t *cls);
  void       (MOD_CALL *uobject_despawn_actor)           (uobject_t *actor);
  uclass_t  *(MOD_CALL *uobject_load_class)              (uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags);
  bool       (MOD_CALL *mount_pak)                       (str_t file_path, int order);
  bool       (MOD_CALL *mount_iostore)                   (str_t file_path, int order);
};

#endif /* MOD_SDK_H */

