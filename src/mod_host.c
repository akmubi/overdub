#include "mod_host.h"

#include "arena.h"
#include "globals.h"
#include "input.h"
#include "mod_manager.h"
#include "scratch.h"
#include "sigscan.h"
#include "unreal.h"

#include <windows.h>

static inline host_tmp_arena_t
host_tmp_arena_make(tmp_arena_t tmp)
{
  return (host_tmp_arena_t){
    .arena = mod_arena_handle_make(tmp.arena),
    .pos   = tmp.pos,
  };
}

static inline tmp_arena_t
host_tmp_arena_resolve(host_tmp_arena_t host_tmp)
{
  return (tmp_arena_t){
    .arena = mod_arena_handle_resolve(host_tmp.arena),
    .pos   = host_tmp.pos,
  };
}

static str_t MOD_CALL
host_get_mod_dir(mod_handle_t h)
{
  return mod_get_mod_dir(&globals.mod_manager, h);
}

static str_t MOD_CALL
host_get_config_path(mod_handle_t h)
{
  return mod_get_config_path(&globals.mod_manager, h);
}

static str_t MOD_CALL
host_get_manifest_path(mod_handle_t h)
{
  return mod_get_manifest_path(&globals.mod_manager, h);
}

static void MOD_CALL
host_logv(mod_handle_t h, log_level_t level, const char *fmt, va_list args)
{
  (void)h;
  log_emitv(level, LOG_SINK_DEFAULT, fmt, args);
}

static mod_cfg_handle_t MOD_CALL
host_get_cfg_by_id(mod_handle_t h, str_t id)
{
  return mod_cfg_get_by_id(&globals.mod_manager, h, id);
}

static bool MOD_CALL
host_register_cmd(mod_handle_t h, str_t name, str_t description, mod_cmd_fn_t fn, void *user)
{
  return mod_register_cmd(&globals.mod_manager, h, name, description, fn, user);
}

static struct nk_context *MOD_CALL
host_get_nk_ctx(void)
{
  return globals.ui_manager.ctx;
}

static void MOD_CALL
host_get_viewport_size(unsigned int *vw, unsigned int *vh)
{
  if (vw) {
    *vw = globals.ui_manager.vw;
  }

  if (vh) {
    *vh = globals.ui_manager.vh;
  }
}

static void MOD_CALL
host_cfg_get_bool(mod_cfg_handle_t h, bool *val)
{
  if (val) {
    *val = mod_cfg_get_bool(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_get_int(mod_cfg_handle_t h, int *val)
{
  if (val) {
    *val = mod_cfg_get_int(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_get_float(mod_cfg_handle_t h, float *val)
{
  if (val) {
    *val = mod_cfg_get_float(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_get_enum(mod_cfg_handle_t h, int *val)
{
  if (val) {
    *val = mod_cfg_get_enum(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_get_string_len(mod_cfg_handle_t h, uint64_t *len)
{
  if (len) {
    *len = mod_cfg_get_string_len(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_get_string_data(mod_cfg_handle_t h, void *buf, uint64_t cap, uint64_t *written)
{
  uint64_t len = mod_cfg_get_string_data(&globals.mod_manager, h, buf, cap);
  if (written) {
    *written = len;
  }
}

static void MOD_CALL
host_cfg_get_keybind(mod_cfg_handle_t h, keybind_t *val)
{
  if (val) {
    *val = mod_cfg_get_keybind(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_get_color(mod_cfg_handle_t h, mod_color_t *val)
{
  if (val) {
    *val = mod_cfg_get_color(&globals.mod_manager, h);
  }
}

static void MOD_CALL
host_cfg_set_bool(mod_cfg_handle_t h, bool val)
{
  mod_cfg_set_bool(&globals.mod_manager, h, val);
}

static void MOD_CALL
host_cfg_set_int(mod_cfg_handle_t h, int val)
{
  mod_cfg_set_int(&globals.mod_manager, h, val);
}

static void MOD_CALL
host_cfg_set_float(mod_cfg_handle_t h, float val)
{
  mod_cfg_set_float(&globals.mod_manager, h, val);
}

static void MOD_CALL
host_cfg_set_enum(mod_cfg_handle_t h, int val)
{
  mod_cfg_set_enum(&globals.mod_manager, h, val);
}

static void MOD_CALL
host_cfg_set_string(mod_cfg_handle_t h, str_t val)
{
  mod_cfg_set_string(&globals.mod_manager, h, val);
}

static void MOD_CALL
host_cfg_set_keybind(mod_cfg_handle_t h, keybind_t val)
{
  mod_cfg_set_keybind(&globals.mod_manager, h, val);
}

static void MOD_CALL
host_cfg_set_color(mod_cfg_handle_t h, mod_color_t val)
{
  mod_cfg_set_color(&globals.mod_manager, h, val);
}

static mod_arena_handle_t MOD_CALL
host_get_perm(mod_handle_t h)
{
  return mod_get_perm_arena(&globals.mod_manager, h);
}

static mod_arena_handle_t MOD_CALL
host_arena_create(mod_handle_t h, uint64_t reserve_size, uint64_t commit_size)
{
  return mod_dll_arena_alloc(&globals.mod_manager, h, reserve_size, commit_size);
}

static void MOD_CALL
host_arena_destroy(mod_handle_t h, mod_arena_handle_t arena_h)
{
  mod_dll_arena_free(&globals.mod_manager, h, arena_h);
}

static void *MOD_CALL
host_arena_push(mod_arena_handle_t h, uint64_t size, uint64_t alignment)
{
  if (h == MOD_ARENA_HANDLE_INVALID) {
    return NULL;
  }

  return arena_push_aligned(mod_arena_handle_resolve(h), size, alignment);
}

static uint64_t MOD_CALL
host_arena_pos(mod_arena_handle_t h)
{
  if (h == MOD_ARENA_HANDLE_INVALID) {
    return 0;
  }

  return arena_get_used(mod_arena_handle_resolve(h));
}

static void MOD_CALL
host_arena_pop_to(mod_arena_handle_t h, uint64_t pos)
{
  if (h == MOD_ARENA_HANDLE_INVALID) {
    return;
  }

  arena_pop_to(mod_arena_handle_resolve(h), pos);
}

static void MOD_CALL
host_arena_reset(mod_arena_handle_t h)
{
  if (h == MOD_ARENA_HANDLE_INVALID) {
    return;
  }

  arena_reset(mod_arena_handle_resolve(h));
}

static host_tmp_arena_t MOD_CALL
host_scratch_begin(mod_arena_handle_t conflict)
{
  tmp_arena_t tmp = scratch_begin(mod_arena_handle_resolve(conflict));
  return host_tmp_arena_make(tmp);
}

static void MOD_CALL
host_scratch_end(host_tmp_arena_t host_tmp)
{
  tmp_arena_t tmp = host_tmp_arena_resolve(host_tmp);
  scratch_end(tmp);
}

static fname_pool_t *MOD_CALL
host_get_name_pool(void)
{
  return globals.name_pool;
}

static fuobject_array_t *MOD_CALL
host_get_object_array(void)
{
  return globals.uobjects;
}

static uworld_t **MOD_CALL
host_get_gworld_ptr(void)
{
  return globals.gworld_ptr;
}

static void MOD_CALL
host_process_event(uobject_t *self, ufunc_t *func, void *params)
{
  unreal_process_event(self, func, params);
}

static uclass_t *MOD_CALL
host_uobject_load_class(uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags)
{
  return unreal_load_class(base_cls, outer, name, filename, load_flags);
}

static bool MOD_CALL
host_mount_pak(str_t file_path, int order)
{
  return unreal_mount_pak(file_path, order);
}

static bool MOD_CALL
host_mount_iostore(str_t file_path, int order)
{
  return unreal_mount_iostore(file_path, order);
}

static fname_t MOD_CALL
host_fname_from_str(str_t str, efind_name_t find_type)
{
  return unreal_fname_from_str(str, find_type);
}

static uint64_t MOD_CALL
host_fname_utf8_len(fname_t name)
{
  return unreal_fname_utf8_len(name);
}

static uint64_t MOD_CALL
host_fname_utf8_write(fname_t name, void *buf, uint64_t cap)
{
  return unreal_fname_utf8_write(buf, cap, name);
}

static void *MOD_CALL
host_get_mcast_sparse_delegate(uobject_t *delegate_owner, fname_t delegate_name)
{
  return unreal_get_mcast_sparse_delegate(delegate_owner, delegate_name);
}

static fnative_func_ptr_t MOD_CALL
host_get_native(uint8_t opcode)
{
  return unreal_get_native(opcode);
}

static input_key_kind_t MOD_CALL
host_input_key_from_str(str_t name)
{
  return input_key_from_str(name);
}

static str_t MOD_CALL
host_input_key_to_str(input_key_kind_t key)
{
  return input_key_to_str(key);
}

static str_t MOD_CALL
host_input_event_kind_to_str(input_event_kind_t kind)
{
  return input_event_kind_to_str(kind);
}

static bool MOD_CALL
host_keybind_parse(str_t str, keybind_t *out)
{
  keybind_t tmp = keybind_parse(str, KEYBIND_NULL);
  if (!keybind_is_valid(tmp)) {
    return false;
  }

  if (out) {
    *out = tmp;
  }
  return true;
}

static uint64_t MOD_CALL
host_keybind_utf8_len(keybind_t bind)
{
  return keybind_utf8_len(bind);
}

static uint64_t MOD_CALL
host_keybind_utf8_write(keybind_t bind, void *buf, uint64_t cap)
{
  return keybind_utf8_write(bind, buf, cap);
}

static bool MOD_CALL
host_keybind_is_down(keybind_t bind)
{
  return keybind_is_down(bind);
}

static bool MOD_CALL
host_keybind_was_down(keybind_t bind)
{
  return keybind_was_down(bind);
}

static bool MOD_CALL
host_register_uobject_listener(mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user)
{
  return mod_dll_uobject_listener_register(&globals.mod_manager, h, kind, notify_cb, user);
}

static void MOD_CALL
host_deregister_uobject_listener(mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user)
{
  mod_dll_uobject_listener_deregister(&globals.mod_manager, h, kind, notify_cb, user);
}

static sigscan_err_t MOD_CALL
host_sigscan(const char *name, const char *pattern, uint8_t op_offset, uint8_t num_deref, uintptr_t first_rvas_to_try[8], void **result)
{
  sigscan_entry_t entry = {
    .name    = name,
    .pattern = pattern,
    .pp      = result,
    .kind    = (op_offset == 0) ? SIG_DIRECT : SIG_RIPREL32_AT,
    .op_off  = op_offset,
    .deref   = num_deref,
    .addr    = NULL,
  };

  if (first_rvas_to_try) {
    mem_copy(entry.first_rvas, first_rvas_to_try, sizeof(entry.first_rvas));
  }

  return sigscan_scan_entry(globals.user_spans, globals.num_user_spans, globals.user_module_base, &entry);
}

static bool MOD_CALL
host_hook_create(mod_handle_t h, void *target, void *detour, void **original)
{
  return mod_dll_hook_create(&globals.mod_manager, h, target, detour, original);
}

static bool MOD_CALL
host_hook_enable(mod_handle_t h, void *target)
{
  return mod_dll_hook_enable(&globals.mod_manager, h, target);
}

static bool MOD_CALL
host_hook_disable(mod_handle_t h, void *target)
{
  return mod_dll_hook_disable(&globals.mod_manager, h, target);
}

static bool MOD_CALL
host_hook_remove(mod_handle_t h, void *target)
{
  return mod_dll_hook_remove(&globals.mod_manager, h, target);
}

static const mod_host_api_t g_host_api = {
  .struct_size = sizeof(mod_host_api_t),
  .abi_version = MOD_HOST_ABI_VERSION,

  /* MOD */

  .get_mod_dir       = host_get_mod_dir,
  .get_config_path   = host_get_config_path,
  .get_manifest_path = host_get_manifest_path,
  .logv              = host_logv,
  .get_cfg_by_id     = host_get_cfg_by_id,
  .register_cmd      = host_register_cmd,

  /* UI */
  .get_nk_ctx        = host_get_nk_ctx,
  .get_viewport_size = host_get_viewport_size,

  /* MOD CONFIGURATION */

  .cfg_get_bool        = host_cfg_get_bool,
  .cfg_get_int         = host_cfg_get_int,
  .cfg_get_float       = host_cfg_get_float,
  .cfg_get_enum        = host_cfg_get_enum,
  .cfg_get_string_len  = host_cfg_get_string_len,
  .cfg_get_string_data = host_cfg_get_string_data,
  .cfg_get_keybind     = host_cfg_get_keybind,
  .cfg_get_color       = host_cfg_get_color,
  .cfg_set_bool        = host_cfg_set_bool,
  .cfg_set_int         = host_cfg_set_int,
  .cfg_set_float       = host_cfg_set_float,
  .cfg_set_enum        = host_cfg_set_enum,
  .cfg_set_string      = host_cfg_set_string,
  .cfg_set_keybind     = host_cfg_set_keybind,
  .cfg_set_color       = host_cfg_set_color,

  /* MEMORY ALLOCATION */

  .get_perm      = host_get_perm,
  .arena_create  = host_arena_create,
  .arena_destroy = host_arena_destroy,
  .arena_push    = host_arena_push,
  .arena_pos     = host_arena_pos,
  .arena_pop_to  = host_arena_pop_to,
  .arena_reset   = host_arena_reset,
  .scratch_begin = host_scratch_begin,
  .scratch_end   = host_scratch_end,

  /* UNREAL ENGINE */

  .get_name_pool             = host_get_name_pool,
  .get_object_array          = host_get_object_array,
  .get_gworld_ptr            = host_get_gworld_ptr,
  .process_event             = host_process_event,
  .uobject_load_class        = host_uobject_load_class,
  .mount_pak                 = host_mount_pak,
  .mount_iostore             = host_mount_iostore,
  .fname_from_str            = host_fname_from_str,
  .fname_utf8_len            = host_fname_utf8_len,
  .fname_utf8_write          = host_fname_utf8_write,
  .get_mcast_sparse_delegate = host_get_mcast_sparse_delegate,
  .get_native                = host_get_native,

  /* INPUT EVENTS / KEYBINDS */

  .input_key_from_str      = host_input_key_from_str,
  .input_key_to_str        = host_input_key_to_str,
  .input_event_kind_to_str = host_input_event_kind_to_str,
  .keybind_parse           = host_keybind_parse,
  .keybind_utf8_len        = host_keybind_utf8_len,
  .keybind_utf8_write      = host_keybind_utf8_write,
  .keybind_is_down         = host_keybind_is_down,
  .keybind_was_down        = host_keybind_was_down,

  /* UOBJECT CREATE/DELETE LISTENERS */

  .register_uobject_listener   = host_register_uobject_listener,
  .deregister_uobject_listener = host_deregister_uobject_listener,

  /* HOOKING / SIGNATURE SCANNING */
  .sigscan      = host_sigscan,
  .hook_create  = host_hook_create,
  .hook_enable  = host_hook_enable,
  .hook_disable = host_hook_disable,
  .hook_remove  = host_hook_remove,
};

const mod_host_api_t *
mod_host_api_get(void)
{
  return &g_host_api;
}
