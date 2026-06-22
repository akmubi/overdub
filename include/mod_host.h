#ifndef MOD_HOST_H
#define MOD_HOST_H

#include "arena.h"
#include "log.h"
#include "str.h"
#include "types.h"

#include "sigscan.h"
#include "unreal.h"

#include "mod_manager.h"

#define MOD_HOST_ABI_VERSION MAKE_VERSION(1, 0, 0)

typedef struct host_tmp_arena_s host_tmp_arena_t;
struct host_tmp_arena_s {
  mod_arena_handle_t arena;
  uint64_t           pos;
};

typedef struct mod_host_api_s mod_host_api_t;
struct mod_host_api_s {
  uint32_t  struct_size;
  version_t abi_version;

  /* MOD */

  str_t            (MOD_CALL *get_mod_dir)        (mod_handle_t h);
  str_t            (MOD_CALL *get_config_path)    (mod_handle_t h);
  str_t            (MOD_CALL *get_manifest_path)  (mod_handle_t h);
  void             (MOD_CALL *logv)               (mod_handle_t h, log_level_t level, const char *fmt, va_list args);
  mod_cfg_handle_t (MOD_CALL *get_cfg_by_id)      (mod_handle_t h, str_t id);
  bool             (MOD_CALL *register_cmd)       (mod_handle_t h, str_t name, str_t description, mod_cmd_fn_t fn, void *user);

  /* UI */
  struct nk_context *(MOD_CALL *get_nk_ctx)       (void);
  void               (MOD_CALL *get_viewport_size)(unsigned int *vw, unsigned int *vh);

  /* MOD CONFIGURATION */

  void (MOD_CALL *cfg_get_bool)       (mod_cfg_handle_t h, bool        *val);
  void (MOD_CALL *cfg_get_int)        (mod_cfg_handle_t h, int         *val);
  void (MOD_CALL *cfg_get_float)      (mod_cfg_handle_t h, float       *val);
  void (MOD_CALL *cfg_get_enum)       (mod_cfg_handle_t h, int         *val);
  void (MOD_CALL *cfg_get_string_len) (mod_cfg_handle_t h, uint64_t    *len);
  void (MOD_CALL *cfg_get_string_data)(mod_cfg_handle_t h, void        *buf, uint64_t cap, uint64_t *written);
  void (MOD_CALL *cfg_get_keybind)    (mod_cfg_handle_t h, keybind_t   *val);
  void (MOD_CALL *cfg_get_color)      (mod_cfg_handle_t h, mod_color_t *val);
  void (MOD_CALL *cfg_set_bool)       (mod_cfg_handle_t h, bool         val);
  void (MOD_CALL *cfg_set_int)        (mod_cfg_handle_t h, int          val);
  void (MOD_CALL *cfg_set_float)      (mod_cfg_handle_t h, float        val);
  void (MOD_CALL *cfg_set_enum)       (mod_cfg_handle_t h, int          val);
  void (MOD_CALL *cfg_set_string)     (mod_cfg_handle_t h, str_t        val);
  void (MOD_CALL *cfg_set_keybind)    (mod_cfg_handle_t h, keybind_t    val);
  void (MOD_CALL *cfg_set_color)      (mod_cfg_handle_t h, mod_color_t  val);

  /* MEMORY ALLOCATION */

  mod_arena_handle_t (MOD_CALL *get_perm)     (mod_handle_t       h);
  mod_arena_handle_t (MOD_CALL *arena_create) (mod_handle_t       h, uint64_t reserve_size, uint64_t commit_size);
  void               (MOD_CALL *arena_destroy)(mod_handle_t       h, mod_arena_handle_t arena_h);
  void              *(MOD_CALL *arena_push)   (mod_arena_handle_t h, uint64_t size, uint64_t alignment);
  uint64_t           (MOD_CALL *arena_pos)    (mod_arena_handle_t h);
  void               (MOD_CALL *arena_pop_to) (mod_arena_handle_t h, uint64_t pos);
  void               (MOD_CALL *arena_reset)  (mod_arena_handle_t h);
  host_tmp_arena_t   (MOD_CALL *scratch_begin)(mod_arena_handle_t conflict);
  void               (MOD_CALL *scratch_end)  (host_tmp_arena_t s);

  /* UNREAL ENGINE */

  fname_pool_t      *(MOD_CALL *get_name_pool)            (void);
  fuobject_array_t  *(MOD_CALL *get_object_array)         (void);
  uworld_t         **(MOD_CALL *get_gworld_ptr)           (void);
  void               (MOD_CALL *process_event)            (uobject_t *self, ufunc_t *func, void *params);
  uclass_t          *(MOD_CALL *uobject_load_class)       (uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags);
  bool               (MOD_CALL *mount_pak)                (str_t file_path, int order);
  bool               (MOD_CALL *mount_iostore)            (str_t file_path, int order);
  fname_t            (MOD_CALL *fname_from_str)           (str_t str, efind_name_t find_type);
  uint64_t           (MOD_CALL *fname_utf8_len)           (fname_t fname);
  uint64_t           (MOD_CALL *fname_utf8_write)         (fname_t fname, void *buf, uint64_t cap);
  void              *(MOD_CALL *get_mcast_sparse_delegate)(uobject_t *delegate_owner, fname_t delegate_name);
  fnative_func_ptr_t (MOD_CALL *get_native)               (uint8_t opcode);

  /* INPUT EVENTS / KEYBINDS */

  input_key_kind_t (MOD_CALL *input_key_from_str)     (str_t name);
  str_t            (MOD_CALL *input_key_to_str)       (input_key_kind_t key);
  str_t            (MOD_CALL *input_event_kind_to_str)(input_event_kind_t kind);
  bool             (MOD_CALL *keybind_parse)          (str_t str, keybind_t *out);
  uint64_t         (MOD_CALL *keybind_utf8_len)       (keybind_t bind);
  uint64_t         (MOD_CALL *keybind_utf8_write)     (keybind_t bind, void *buf, uint64_t cap);
  bool             (MOD_CALL *keybind_is_down)        (keybind_t bind);
  bool             (MOD_CALL *keybind_was_down)       (keybind_t bind);

  /* UOBJECT CREATE/DELETE LISTENERS */

  bool             (MOD_CALL *register_uobject_listener)  (mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user);
  void             (MOD_CALL *deregister_uobject_listener)(mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user);


  /* HOOKING / SIGNATURE SCANNING */

  sigscan_err_t (MOD_CALL *sigscan)     (const char *name, const char *pattern, uint8_t op_offset, uint8_t num_deref, uintptr_t first_rvas_to_try[8], void **result);
  bool          (MOD_CALL *hook_create) (mod_handle_t h, void *target, void *detour, void **original);
  bool          (MOD_CALL *hook_enable) (mod_handle_t h, void *target);
  bool          (MOD_CALL *hook_disable)(mod_handle_t h, void *target);
  bool          (MOD_CALL *hook_remove) (mod_handle_t h, void *target);
};

const mod_host_api_t *
mod_host_api_get(void);

typedef struct mod_api_s mod_api_t;
struct mod_api_s {
  uint32_t                  struct_size;
  version_t                 abi_version;
  mod_init_fn_t             init;
  mod_deinit_fn_t           deinit;
  mod_tick_fn_t             tick;
  mod_input_fn_t            input;
  mod_pe_pre_fn_t           pe_pre;
  mod_pe_post_fn_t          pe_post;
  mod_func_invoke_pre_fn_t  func_invoke_pre;
  mod_func_invoke_post_fn_t func_invoke_post;
  mod_draw_panel_fn_t       draw_panel;
  mod_draw_config_fn_t      draw_config;
};

typedef const mod_api_t *(MOD_CALL *mod_entry_fn_t)(void);

/*
 * Strict host/plugin ABI rule:
 * - major must match
 * - minor must match
 * - patch is ignored for compatibility
 */
static inline bool
mod_abi_compatible(version_t host, version_t plugin)
{
  return host.major == plugin.major && host.minor == plugin.minor;
}

#endif /* MOD_HOST_H */
