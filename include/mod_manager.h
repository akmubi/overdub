#ifndef MOD_MANAGER_H
#define MOD_MANAGER_H

#include "arena.h"
#include "ini.h"
#include "input.h"
#include "str.h"
#include "types.h"
#include "unreal.h"
#include "version.h"

#include "vendor_nuklear.h"

#define MOD_HANDLE_INVALID       ((mod_handle_t)0)
#define MOD_CFG_HANDLE_INVALID   ((mod_cfg_handle_t)0)
#define MOD_ARENA_HANDLE_INVALID ((mod_arena_handle_t)0)

typedef uint64_t mod_handle_t;
typedef uint64_t mod_cfg_handle_t;
typedef uint64_t mod_arena_handle_t;

#define MOD_CALL             __cdecl
#define MOD_DLL_ENTRY_EXPORT "mod_entry"

typedef struct mod_host_api_s mod_host_api_t;

typedef bool(MOD_CALL *mod_init_fn_t)(const mod_host_api_t *host, mod_handle_t h);
typedef void(MOD_CALL *mod_deinit_fn_t)(mod_handle_t h);

typedef void(MOD_CALL *mod_tick_fn_t)(mod_handle_t h, float delta);
typedef bool(MOD_CALL *mod_input_fn_t)(mod_handle_t h, input_event_t *ev);

typedef bool(MOD_CALL *mod_pe_pre_fn_t)(mod_handle_t h, uobject_t *obj, ufunc_t *func, void *params);
typedef void(MOD_CALL *mod_pe_post_fn_t)(mod_handle_t h, uobject_t *obj, ufunc_t *func, void *params, bool consumed);
typedef bool(MOD_CALL *mod_func_invoke_pre_fn_t)(mod_handle_t h, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result);
typedef void(MOD_CALL *mod_func_invoke_post_fn_t)(mod_handle_t h, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result, bool consumed);
typedef void(MOD_CALL *mod_draw_panel_fn_t) (mod_handle_t h, struct nk_context *ctx);
typedef void(MOD_CALL *mod_draw_config_fn_t)(mod_handle_t h, struct nk_context *ctx);
typedef void(MOD_CALL *mod_cmd_fn_t)(mod_handle_t h, str_t name, str_t args, void *user);

typedef uint8_t mod_kind_t;
enum {
  MOD_KIND_MOD,
  MOD_KIND_TOOL,
  MOD_KIND_MAX,
};

static inline str_t
mod_kind_to_str(mod_kind_t kind)
{
  switch (kind) {
  case MOD_KIND_MOD:
    return STR_LIT("mod");
  case MOD_KIND_TOOL:
    return STR_LIT("tool");
  }
  return STR_LIT("unknown");
}

typedef uint8_t mod_option_type_t;
enum {
  MOD_OPTION_NONE = 0,
  MOD_OPTION_BOOL,
  MOD_OPTION_INT,
  MOD_OPTION_FLOAT,
  MOD_OPTION_ENUM,
  MOD_OPTION_STRING,
  MOD_OPTION_KEYBIND,
  MOD_OPTION_COLOR,
  MOD_OPTION_MAX,
};

typedef uint8_t mod_spawn_context_kind_t;
enum {
  MOD_SPAWN_CONTEXT_NONE,
  MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER,
  MOD_SPAWN_CONTEXT_LOCAL_PLAYER,
  MOD_SPAWN_CONTEXT_PAWN,
  MOD_SPAWN_CONTEXT_HUD,
  MOD_SPAWN_CONTEXT_WORLD,
  MOD_SPAWN_CONTEXT_GAME_INSTANCE,
  MOD_SPAWN_CONTEXT_CUSTOM,
  MOD_SPAWN_CONTEXT_MAX,
};

static inline str_t
mod_spawn_context_kind_to_str(mod_spawn_context_kind_t kind)
{
  switch (kind) {
  case MOD_SPAWN_CONTEXT_NONE:
    return STR_LIT("none");
  case MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER:
    return STR_LIT("player_controller");
  case MOD_SPAWN_CONTEXT_LOCAL_PLAYER:
    return STR_LIT("local_player");
  case MOD_SPAWN_CONTEXT_PAWN:
    return STR_LIT("pawn");
  case MOD_SPAWN_CONTEXT_HUD:
    return STR_LIT("hud");
  case MOD_SPAWN_CONTEXT_WORLD:
    return STR_LIT("world");
  case MOD_SPAWN_CONTEXT_GAME_INSTANCE:
    return STR_LIT("game_instance");
  case MOD_SPAWN_CONTEXT_CUSTOM:
    return STR_LIT("custom");
  }
  return STR_LIT("unknown");
}

typedef struct mod_info_s mod_info_t;
struct mod_info_s {
  str_t      id;
  str_t      name;
  str_t      author;
  str_t      description;
  mod_kind_t kind;
  version_t  version;
};

typedef struct mod_dll_info_s mod_dll_info_t;
struct mod_dll_info_s {
  str_t path;
};

typedef struct mod_asset_info_s mod_asset_info_t;
struct mod_asset_info_s {
  str_t pak_path;
  str_t utoc_path;
  str_t ucas_path;
};

typedef struct mod_blueprint_info_s mod_blueprint_info_t;
struct mod_blueprint_info_s {
  str_t                    id;
  str_t                    mod_actor_class_path;
  mod_spawn_context_kind_t attach_to;
  str_t                    custom_attach_class_path; // required when attach_to is MOD_SPAWN_CONTEXT_CUSTOM
  bool                     auto_spawn;
  keybind_t                default_spawn_keybind;
};

typedef struct mod_color_s mod_color_t;
struct mod_color_s {
  uint8_t r, g, b, a;
};

typedef struct mod_option_info_s mod_option_info_t;
struct mod_option_info_s {
  mod_option_type_t type;
  str_t             id;
  str_t             label;
  str_t             description;

  union {
    struct {
      bool default_val;
    } boolean;

    struct {
      int default_val;
      int min_val;
      int max_val;
      int step;
    } integer;

    struct {
      float default_val;
      float min_val;
      float max_val;
      float step;
    } floating;

    struct {
      str_array_t values;
      str_t       default_val;
    } enumeration;

    struct {
      str_t default_val;
    } string;

    struct {
      keybind_t default_val;
    } keybind;

    struct {
      mod_color_t default_val;
    } color;
  } val;
};

typedef struct mod_manifest_s mod_manifest_t;
struct mod_manifest_s {
  str_t mod_dir_name;
  str_t mod_dir;
  str_t manifest_path;
  str_t config_path;

  mod_info_t            info;
  mod_dll_info_t        dll;
  mod_asset_info_t      asset;
  mod_blueprint_info_t *blueprints;
  int                   blueprint_count;
  mod_option_info_t    *options;
  int                   option_count;
};

typedef uint8_t mod_dll_state_t;
enum {
  MOD_DLL_STATE_UNLOADED = 0,
  MOD_DLL_STATE_LOADED   = 1,
};

static inline str_t
mod_dll_state_to_str(mod_dll_state_t state)
{
  switch (state) {
  case MOD_DLL_STATE_UNLOADED:
    return STR_LIT("unloaded");
  case MOD_DLL_STATE_LOADED:
    return STR_LIT("loaded");
  }
  return STR_LIT("unknown");
}

typedef uint8_t mod_dll_error_stage_t;
enum {
  MOD_DLL_ERROR_NONE = 0,
  MOD_DLL_ERROR_LOAD,
  MOD_DLL_ERROR_INIT,
  MOD_DLL_ERROR_DEINIT,
  MOD_DLL_ERROR_UNLOAD,
};

typedef uint8_t mod_asset_state_t;
enum {
  MOD_ASSET_STATE_UNMOUNTED = 0,
  MOD_ASSET_STATE_MOUNTED   = 1,
  MOD_ASSET_STATE_ERROR,
};

static inline str_t
mod_asset_state_to_str(mod_asset_state_t state)
{
  switch (state) {
  case MOD_ASSET_STATE_UNMOUNTED:
    return STR_LIT("unmounted");
  case MOD_ASSET_STATE_MOUNTED:
    return STR_LIT("mounted");
  case MOD_ASSET_STATE_ERROR:
    return STR_LIT("error");
  }
  return STR_LIT("unknown");
}

typedef uint8_t mod_blueprint_error_stage_t;
enum {
  MOD_BP_ERROR_NONE = 0,
  MOD_BP_ERROR_RESOLVE_CONTEXT,
  MOD_BP_ERROR_LOAD_CLASS,
  MOD_BP_ERROR_SPAWN,
  MOD_BP_ERROR_DESPAWN,
};

typedef struct mod_dll_runtime_funcs_s mod_dll_runtime_funcs_t;
struct mod_dll_runtime_funcs_s {
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

typedef struct mod_dll_runtime_hook_s mod_dll_runtime_hook_t;
struct mod_dll_runtime_hook_s {
  bool   enabled;
  void  *target;
  void  *detour;
  void **original;
};

typedef struct err_msg_s err_msg_t;
struct err_msg_s {
  char msg[CONFIG_MOD_MANAGER_ERROR_MSG_MAX_LEN];
  int  len;
};

typedef struct mod_cmd_s mod_cmd_t;
struct mod_cmd_s {
  str_t        name;
  str_t        description;
  mod_cmd_fn_t cb;
  void        *user;
};

typedef struct mod_dll_arena_s mod_dll_arena_t;
struct mod_dll_arena_s {
  arena_t          arena;
  mod_dll_arena_t *next;
  bool             occupied;
};

typedef struct mod_dll_runtime_s mod_dll_runtime_t;
struct mod_dll_runtime_s {
  mod_dll_state_t         state;
  bool                    active;
  mod_dll_error_stage_t   err_stage;
  err_msg_t               err_msg;
  bool                    is_builtin;
  void                   *dll_handle; // HMODULE on Windows
  mod_dll_runtime_funcs_t funcs;
  arena_t                 perm;
  mod_cmd_t              *commands;
  int                     command_count;
  mod_dll_runtime_hook_t *hooks;
  int                     hook_count;
  mod_dll_arena_t        *arenas;
  mod_dll_arena_t        *arena_first_free;
  uobject_listener_t     *listeners;
};

typedef struct mod_asset_runtime_s mod_asset_runtime_t;
struct mod_asset_runtime_s {
  mod_asset_state_t state;
  str_t             pak_path;
  str_t             utoc_path;
  str_t             ucas_path;
  int               priority;
  err_msg_t         last_error;
};

typedef struct mod_blueprint_cfg_s mod_blueprint_cfg_t;
struct mod_blueprint_cfg_s {
  bool      auto_spawn;
  keybind_t spawn_keybind;
};

typedef struct mod_blueprint_runtime_s mod_blueprint_runtime_t;
struct mod_blueprint_runtime_s {
  mod_blueprint_cfg_t         cfg;
  mod_blueprint_cfg_t         saved_cfg;
  bool                        active;
  uobject_t                  *actor;
  mod_blueprint_error_stage_t err_stage;
  err_msg_t                   err_msg;
};

typedef struct mod_cfg_string_s mod_cfg_string_t;
struct mod_cfg_string_s {
  char     data[CONFIG_MOD_CFG_MAX_STR_LEN];
  uint64_t len;
};

typedef struct mod_option_cfg_s mod_option_cfg_t;
struct mod_option_cfg_s {
  union {
    bool             boolean;
    int              integer;
    float            floating;
    int              enum_item_id; // -1 means no selection
    mod_cfg_string_t string;
    keybind_t        keybind;
    mod_color_t      color;
  };
};

typedef struct mod_option_runtime_s mod_option_runtime_t;
struct mod_option_runtime_s {
  uint16_t         idx;
  uint16_t         mod_idx;
  mod_option_cfg_t cfg;
  mod_option_cfg_t saved_cfg;
};

typedef struct mod_s mod_t;
struct mod_s {
  uint16_t       idx;
  mod_kind_t     kind;
  mod_manifest_t manifest;
  str_t          mod_dir;
  bool           enabled;

  bool has_code;
  bool has_assets;
  bool has_blueprints;
  bool has_options;

  mod_dll_runtime_t        dll;
  mod_asset_runtime_t      asset;
  mod_blueprint_runtime_t *blueprints;
  int                      blueprint_count;
  mod_option_runtime_t    *options;
  int                      option_count;
};

typedef struct mod_manager_mod_order_s mod_manager_mod_order_t;
struct mod_manager_mod_order_s {
  str_array_t  initial_ids;
  mod_handle_t want[CONFIG_MOD_MANAGER_MAX_MODS];
  mod_handle_t runtime[CONFIG_MOD_MANAGER_MAX_MODS];
  int          count;
};

typedef struct mod_manager_cfg_s mod_manager_cfg_t;
struct mod_manager_cfg_s {
  mod_cfg_string_t root_mod_dir;
  keybind_t        overlay_toggle;
};

typedef struct mod_manager_s mod_manager_t;
struct mod_manager_s {
  arena_t                 perm;
  str_t                   game_dir;
  str_t                   config_path;
  mod_manager_mod_order_t mod_order;
  str_array_t             disabled_mods;
  mod_manager_cfg_t       cfg;
  mod_manager_cfg_t       saved_cfg;
  mod_t                  *mods;
  int                     mod_count;
};

void
mod_manager_init(mod_manager_t *manager, str_t game_dir);
void
mod_manager_startup_load_cfg(mod_manager_t *manager);
void
mod_manager_startup_load_mods(mod_manager_t *manager);

void
mod_manager_mount_assets(mod_manager_t *manager);
void
mod_manager_start_dlls(mod_manager_t *manager);
void
mod_manager_start_blueprints(mod_manager_t *manager);

void
mod_manager_save_mod_cfg_all(mod_manager_t *manager);

bool
mod_manager_load_cfg(mod_manager_t *manager);
bool
mod_manager_save_cfg(mod_manager_t *manager);

mod_handle_t
mod_manager_find_mod(mod_manager_t *manager, str_t mod_id);

bool
mod_get_info(mod_manager_t *manager, mod_handle_t h, mod_info_t *info);
bool
mod_get_dll_info(mod_manager_t *manager, mod_handle_t h, mod_dll_info_t *info);
bool
mod_get_asset_info(mod_manager_t *manager, mod_handle_t h, mod_asset_info_t *info);
int
mod_get_blueprints_info(mod_manager_t *manager, mod_handle_t h, mod_blueprint_info_t *out, int cap);
int
mod_get_options_info(mod_manager_t *manager, mod_handle_t h, mod_option_info_t *out, int cap);
str_t
mod_get_mod_dir(mod_manager_t *manager, mod_handle_t h);
str_t
mod_get_config_path(mod_manager_t *manager, mod_handle_t h);
str_t
mod_get_manifest_path(mod_manager_t *manager, mod_handle_t h);
mod_arena_handle_t
mod_get_perm_arena(mod_manager_t *manager, mod_handle_t h);

mod_handle_t
mod_register(mod_manager_t *manager, mod_manifest_t *manifest);
mod_handle_t
mod_register_builtin(mod_manager_t *manager, mod_manifest_t *manifest, mod_dll_runtime_funcs_t funcs);
bool
mod_register_cmd(mod_manager_t *manager, mod_handle_t h, str_t name, str_t description, mod_cmd_fn_t fn, void *user);

bool
mod_dll_load(mod_manager_t *manager, mod_handle_t h);
void
mod_dll_unload(mod_manager_t *manager, mod_handle_t h);
bool
mod_dll_start(mod_manager_t *manager, mod_handle_t h);
bool
mod_dll_stop(mod_manager_t *manager, mod_handle_t h);
bool
mod_dll_restart(mod_manager_t *manager, mod_handle_t h);
bool
mod_dll_reload(mod_manager_t *manager, mod_handle_t h);

bool
mod_blueprint_spawn(mod_manager_t *manager, mod_handle_t h, int idx);
bool
mod_blueprint_despawn(mod_manager_t *manager, mod_handle_t h, int idx);
bool
mod_blueprint_start(mod_manager_t *manager, mod_handle_t h, int idx);
bool
mod_blueprint_stop(mod_manager_t *manager, mod_handle_t h, int idx);
bool
mod_blueprint_cfg_reset(mod_manager_t *manager, mod_handle_t h, int idx);
void
mod_blueprint_tick(mod_manager_t *manager, mod_handle_t h, float delta);

bool
mod_dll_hook_create(mod_manager_t *manager, mod_handle_t h, void *target, void *detour, void **original);
bool
mod_dll_hook_enable(mod_manager_t *manager, mod_handle_t h, void *target);
bool
mod_dll_hook_disable(mod_manager_t *manager, mod_handle_t h, void *target);
bool
mod_dll_hook_remove(mod_manager_t *manager, mod_handle_t h, void *target);

bool
mod_dll_uobject_listener_register(mod_manager_t *manager, mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user);
void
mod_dll_uobject_listener_deregister(mod_manager_t *manager, mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user);

mod_arena_handle_t
mod_dll_arena_alloc(mod_manager_t *manager, mod_handle_t h, uint64_t reserve_size, uint64_t commit_size);
bool
mod_dll_arena_free(mod_manager_t *manager, mod_handle_t h, mod_arena_handle_t arena_h);

void
mod_manager_dispatch_tick(mod_manager_t *manager, float delta);
bool
mod_manager_dispatch_input(mod_manager_t *manager, input_event_t *ev);
bool
mod_manager_dispatch_process_event_pre(mod_manager_t *manager, uobject_t *obj, ufunc_t *func, void *params);
void
mod_manager_dispatch_process_event_post(mod_manager_t *manager, uobject_t *obj, ufunc_t *func, void *params, bool consumed);
bool
mod_manager_dispatch_ufunction_invoke_pre(mod_manager_t *manager, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result);
void
mod_manager_dispatch_ufunction_invoke_post(mod_manager_t *manager, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result, bool consumed);
bool
mod_manager_dispatch_command(mod_manager_t *manager, str_t name, str_t args);

bool
mod_cfg_load(mod_manager_t *manager, mod_handle_t h);
bool
mod_cfg_save(mod_manager_t *manager, mod_handle_t h);
bool
mod_cfg_reset(mod_manager_t *manager, mod_handle_t h);
bool
mod_cfg_revert(mod_manager_t *manager, mod_handle_t h);
mod_cfg_handle_t
mod_cfg_get_by_id(mod_manager_t *manager, mod_handle_t h, str_t id);
bool
mod_cfg_get_bool(mod_manager_t *manager, mod_cfg_handle_t h);
int
mod_cfg_get_int(mod_manager_t *manager, mod_cfg_handle_t h);
float
mod_cfg_get_float(mod_manager_t *manager, mod_cfg_handle_t h);
int
mod_cfg_get_enum(mod_manager_t *manager, mod_cfg_handle_t h);
uint64_t
mod_cfg_get_string_len(mod_manager_t *manager, mod_cfg_handle_t h);
uint64_t
mod_cfg_get_string_data(mod_manager_t *manager, mod_cfg_handle_t h, void *buf, uint64_t cap);
str_t
mod_cfg_get_string(mod_manager_t *manager, mod_cfg_handle_t h, arena_t *arena);
keybind_t
mod_cfg_get_keybind(mod_manager_t *manager, mod_cfg_handle_t h);
mod_color_t
mod_cfg_get_color(mod_manager_t *manager, mod_cfg_handle_t h);
void
mod_cfg_set_bool(mod_manager_t *manager, mod_cfg_handle_t h, bool val);
void
mod_cfg_set_int(mod_manager_t *manager, mod_cfg_handle_t h, int val);
void
mod_cfg_set_float(mod_manager_t *manager, mod_cfg_handle_t h, float val);
void
mod_cfg_set_enum(mod_manager_t *manager, mod_cfg_handle_t h, int val);
void
mod_cfg_set_string(mod_manager_t *manager, mod_cfg_handle_t h, str_t val);
void
mod_cfg_set_keybind(mod_manager_t *manager, mod_cfg_handle_t h, keybind_t bind);
void
mod_cfg_set_color(mod_manager_t *manager, mod_cfg_handle_t h, mod_color_t color);

bool
mod_cfg_string_set(mod_cfg_string_t *dst, str_t src);
str_t
mod_cfg_string_as_str(mod_cfg_string_t *s);

mod_t *
mod_manager_mod_from_owner(mod_manager_t *manager, uint64_t owner);

bool
mod_handle_is_valid(mod_manager_t *manager, mod_handle_t h);
mod_handle_t
mod_handle_make(mod_t *m);
mod_t *
mod_handle_resolve(mod_manager_t *manager, mod_handle_t h);

bool
mod_cfg_handle_is_valid(mod_manager_t *manager, mod_cfg_handle_t h);
mod_cfg_handle_t
mod_cfg_handle_make(mod_option_runtime_t *rt);
mod_option_runtime_t *
mod_cfg_handle_resolve(mod_manager_t *manager, mod_cfg_handle_t h);

mod_arena_handle_t
mod_arena_handle_make(arena_t *arena);
arena_t *
mod_arena_handle_resolve(mod_arena_handle_t h);

void
mod_manager_apply_order_from_config(mod_manager_t *manager, str_array_t mod_order_ids);
bool
mod_has_any_errors(mod_t *m);
bool
mod_is_active(mod_t *m);

void
mod_manager_mod_set_enabled(mod_manager_t *manager, mod_handle_t h, bool enabled);

bool
mod_blueprint_cfg_is_dirty(mod_blueprint_cfg_t *cfg, mod_blueprint_cfg_t *saved_cfg);
bool
mod_option_cfg_is_dirty(mod_option_info_t *info, mod_option_cfg_t *cfg, mod_option_cfg_t *saved_cfg);
bool
mod_manager_cfg_is_dirty(mod_manager_cfg_t *cfg, mod_manager_cfg_t *saved_cfg);

#endif /* MOD_MANAGER_H */
