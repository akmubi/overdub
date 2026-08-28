#include "mod.h"

#include "arena.h"
#include "globals.h"
#include "input.h"
#include "mod_manager.h"
#include "scratch.h"
#include "sigscan.h"
#include "str.h"
#include "unreal.h"

#include <stdarg.h>

str_t MOD_CALL
mod_get_mod_dir(mod_handle_t mod)
{
  return mod_manager_get_mod_dir(&globals.mod_manager, mod);
}

str_t MOD_CALL
mod_get_config_path(mod_handle_t mod)
{
  return mod_manager_get_config_path(&globals.mod_manager, mod);
}

str_t MOD_CALL
mod_get_manifest_path(mod_handle_t mod)
{
  return mod_manager_get_manifest_path(&globals.mod_manager, mod);
}

void MOD_CALL
mod_logv(mod_handle_t mod, log_level_t level, const char *fmt, va_list args)
{
  (void)mod;
  log_emitv(level, LOG_SINK_DEFAULT, fmt, args);
}

void MOD_CALL
mod_log(mod_handle_t mod, log_level_t level, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  mod_logv(mod, level, fmt, args);
  va_end(args);
}

mod_cfg_handle_t MOD_CALL
mod_get_cfg_by_id(mod_handle_t mod, str_t id)
{
  return mod_cfg_get_by_id(&globals.mod_manager, mod, id);
}

bool MOD_CALL
mod_register_cmd(mod_handle_t mod, str_t name, str_t description, mod_cmd_fn_t fn, void *user)
{
  return mod_manager_register_cmd(&globals.mod_manager, mod, name, description, fn, user);
}

struct nk_context *MOD_CALL
mod_get_nk_ctx(void)
{
  return globals.ui_manager.ctx;
}

void MOD_CALL
mod_get_viewport_size(unsigned int *width, unsigned int *height)
{
  if (width) {
    *width = globals.ui_manager.vw;
  }
  if (height) {
    *height = globals.ui_manager.vh;
  }
}

uint64_t MOD_CALL
mod_get_frame_counter(void)
{
  return globals.frame_counter;
}

float MOD_CALL
mod_get_fps(void)
{
  return globals.fps;
}

bool MOD_CALL
mod_cfg_get_bool(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_bool(&globals.mod_manager, cfg);
}

int MOD_CALL
mod_cfg_get_int(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_int(&globals.mod_manager, cfg);
}

float MOD_CALL
mod_cfg_get_float(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_float(&globals.mod_manager, cfg);
}

int MOD_CALL
mod_cfg_get_enum(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_enum(&globals.mod_manager, cfg);
}

uint64_t MOD_CALL
mod_cfg_get_string_len(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_string_len(&globals.mod_manager, cfg);
}

uint64_t MOD_CALL
mod_cfg_get_string_data(mod_cfg_handle_t cfg, void *buf, uint64_t cap)
{
  return mod_manager_cfg_get_string_data(&globals.mod_manager, cfg, buf, cap);
}

str_t MOD_CALL
mod_cfg_push_string(mod_cfg_handle_t cfg, arena_t *arena)
{
  uint64_t len = mod_cfg_get_string_len(cfg);
  if (len == 0) {
    return STR_NULL;
  }

  uint8_t *data = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, len + 1);
  if (!data) {
    return STR_NULL;
  }

  return str_make(data, mod_cfg_get_string_data(cfg, data, len));
}

keybind_t MOD_CALL
mod_cfg_get_keybind(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_keybind(&globals.mod_manager, cfg);
}

mod_color_t MOD_CALL
mod_cfg_get_color(mod_cfg_handle_t cfg)
{
  return mod_manager_cfg_get_color(&globals.mod_manager, cfg);
}

void MOD_CALL
mod_cfg_set_bool(mod_cfg_handle_t cfg, bool value)
{
  mod_manager_cfg_set_bool(&globals.mod_manager, cfg, value);
}

void MOD_CALL
mod_cfg_set_int(mod_cfg_handle_t cfg, int value)
{
  mod_manager_cfg_set_int(&globals.mod_manager, cfg, value);
}

void MOD_CALL
mod_cfg_set_float(mod_cfg_handle_t cfg, float value)
{
  mod_manager_cfg_set_float(&globals.mod_manager, cfg, value);
}

void MOD_CALL
mod_cfg_set_enum(mod_cfg_handle_t cfg, int value)
{
  mod_manager_cfg_set_enum(&globals.mod_manager, cfg, value);
}

void MOD_CALL
mod_cfg_set_string(mod_cfg_handle_t cfg, str_t value)
{
  mod_manager_cfg_set_string(&globals.mod_manager, cfg, value);
}

void MOD_CALL
mod_cfg_set_keybind(mod_cfg_handle_t cfg, keybind_t value)
{
  mod_manager_cfg_set_keybind(&globals.mod_manager, cfg, value);
}

void MOD_CALL
mod_cfg_set_color(mod_cfg_handle_t cfg, mod_color_t value)
{
  mod_manager_cfg_set_color(&globals.mod_manager, cfg, value);
}

arena_t *MOD_CALL
mod_get_perm(mod_handle_t mod)
{
  return mod_get_perm_arena(&globals.mod_manager, mod);
}

arena_t *MOD_CALL
mod_arena_create(mod_handle_t mod, uint64_t reserve_size, uint64_t commit_size)
{
  return mod_dll_arena_alloc(&globals.mod_manager, mod, reserve_size, commit_size);
}

void MOD_CALL
mod_arena_destroy(mod_handle_t mod, arena_t *arena)
{
  mod_dll_arena_free(&globals.mod_manager, mod, arena);
}

bool MOD_CALL
mod_register_uobject_listener(mod_handle_t mod, uobject_listener_kind_t kind, uobject_on_notify_cb_t callback, void *user)
{
  return mod_dll_uobject_listener_register(&globals.mod_manager, mod, kind, callback, user);
}

void MOD_CALL
mod_deregister_uobject_listener(mod_handle_t mod, uobject_listener_kind_t kind, uobject_on_notify_cb_t callback, void *user)
{
  mod_dll_uobject_listener_deregister(&globals.mod_manager, mod, kind, callback, user);
}

sigscan_err_t MOD_CALL
mod_sigscan(sigscan_entry_t *entry)
{
  if (!entry) {
    return SIG_ERR_NOT_FOUND;
  }
  return sigscan_scan_entry(globals.user_spans, globals.num_user_spans, globals.user_module_base, entry);
}

bool MOD_CALL
mod_hook_create(mod_handle_t mod, void *target, void *detour, void **original)
{
  return mod_dll_hook_create(&globals.mod_manager, mod, target, detour, original);
}

bool MOD_CALL
mod_hook_enable(mod_handle_t mod, void *target)
{
  return mod_dll_hook_enable(&globals.mod_manager, mod, target);
}

bool MOD_CALL
mod_hook_disable(mod_handle_t mod, void *target)
{
  return mod_dll_hook_disable(&globals.mod_manager, mod, target);
}

bool MOD_CALL
mod_hook_remove(mod_handle_t mod, void *target)
{
  return mod_dll_hook_remove(&globals.mod_manager, mod, target);
}
