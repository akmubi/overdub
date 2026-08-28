#ifndef MOD_H
#define MOD_H

#include "input.h"
#include "log.h"
#include "sigscan.h"
#include "str.h"
#include "types.h"
#include "unreal.h"
#include "version.h"

MOD_EXTERN_C_BEGIN

typedef uint64_t mod_handle_t;
typedef uint64_t mod_cfg_handle_t;
typedef version_t mod_version_t;

#define MOD_HANDLE_INVALID     ((mod_handle_t)0)
#define MOD_CFG_HANDLE_INVALID ((mod_cfg_handle_t)0)

#define MOD_ABI_VERSION ((mod_version_t)MAKE_VERSION(2, 0, 0))

#define MOD_ABI_VERSION_EXPORT "mod_abi_version"
#define MOD_ENTRY_EXPORT       "mod_entry"

#define MOD_ABI_VERSION_ENTRY() MOD_EXTERN_C MOD_EXPORT mod_version_t    MOD_CALL mod_abi_version(void)
#define MOD_ENTRY()             MOD_EXTERN_C MOD_EXPORT const mod_api_t *MOD_CALL mod_entry(void)

#define MOD_LOG_ERROR(MOD, ...) mod_log((MOD), LOG_LEVEL_ERROR, __VA_ARGS__)
#define MOD_LOG_WARN(MOD, ...)  mod_log((MOD), LOG_LEVEL_WARN,  __VA_ARGS__)
#define MOD_LOG_INFO(MOD, ...)  mod_log((MOD), LOG_LEVEL_INFO,  __VA_ARGS__)
#define MOD_LOG_DEBUG(MOD, ...) mod_log((MOD), LOG_LEVEL_DEBUG, __VA_ARGS__)

typedef struct mod_color_s mod_color_t;
struct mod_color_s {
  uint8_t r, g, b, a;
};

struct nk_context;

typedef void (MOD_CALL *mod_tick_fn_t) (mod_handle_t mod, float delta);
typedef bool (MOD_CALL *mod_input_fn_t)(mod_handle_t mod, input_event_t *ev);

typedef bool (MOD_CALL *mod_pe_pre_fn_t) (mod_handle_t mod, uobject_t *obj, ufunc_t *func, void *params);
typedef void (MOD_CALL *mod_pe_post_fn_t)(mod_handle_t mod, uobject_t *obj, ufunc_t *func, void *params, bool consumed);

typedef bool (MOD_CALL *mod_func_invoke_pre_fn_t) (mod_handle_t mod, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result);
typedef void (MOD_CALL *mod_func_invoke_post_fn_t)(mod_handle_t mod, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result, bool consumed);

typedef void (MOD_CALL *mod_draw_panel_fn_t) (mod_handle_t mod, struct nk_context *ctx);
typedef void (MOD_CALL *mod_draw_config_fn_t)(mod_handle_t mod, struct nk_context *ctx);

typedef bool (MOD_CALL *mod_init_fn_t)  (mod_handle_t mod);
typedef void (MOD_CALL *mod_deinit_fn_t)(mod_handle_t mod);
typedef void (MOD_CALL *mod_cmd_fn_t)   (mod_handle_t mod, str_t name, str_t args, void *user);

typedef struct mod_api_s mod_api_t;
struct mod_api_s {
  uint32_t                  struct_size;
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

typedef mod_version_t    (MOD_CALL *mod_abi_version_fn_t)(void);
typedef const mod_api_t *(MOD_CALL *mod_entry_fn_t)(void);

/* Mod identity and mod-owned registrations */
MOD_API str_t            mod_get_mod_dir(mod_handle_t mod);
MOD_API str_t            mod_get_config_path(mod_handle_t mod);
MOD_API str_t            mod_get_manifest_path(mod_handle_t mod);
MOD_API void             mod_log(mod_handle_t mod, log_level_t level, const char *fmt, ...) ATTR_FORMAT(3, 4);
MOD_API void             mod_logv(mod_handle_t mod, log_level_t level, const char *fmt, va_list args);
MOD_API mod_cfg_handle_t mod_get_cfg_by_id(mod_handle_t mod, str_t id);
MOD_API bool             mod_register_cmd(mod_handle_t mod, str_t name, str_t description, mod_cmd_fn_t fn, void *user);

/* UI and timing */
MOD_API struct nk_context *mod_get_nk_ctx(void);
MOD_API void               mod_get_viewport_size(unsigned int *width, unsigned int *height);
MOD_API uint64_t           mod_get_frame_counter(void);
MOD_API float              mod_get_fps(void);

/* Manifest-backed configuration */
MOD_API bool        mod_cfg_get_bool(mod_cfg_handle_t cfg);
MOD_API int         mod_cfg_get_int(mod_cfg_handle_t cfg);
MOD_API float       mod_cfg_get_float(mod_cfg_handle_t cfg);
MOD_API int         mod_cfg_get_enum(mod_cfg_handle_t cfg);
MOD_API uint64_t    mod_cfg_get_string_len(mod_cfg_handle_t cfg);
MOD_API uint64_t    mod_cfg_get_string_data(mod_cfg_handle_t cfg, void *buf, uint64_t cap);
MOD_API str_t       mod_cfg_push_string(mod_cfg_handle_t cfg, arena_t *arena);
MOD_API keybind_t   mod_cfg_get_keybind(mod_cfg_handle_t cfg);
MOD_API mod_color_t mod_cfg_get_color(mod_cfg_handle_t cfg);
MOD_API void        mod_cfg_set_bool(mod_cfg_handle_t cfg, bool value);
MOD_API void        mod_cfg_set_int(mod_cfg_handle_t cfg, int value);
MOD_API void        mod_cfg_set_float(mod_cfg_handle_t cfg, float value);
MOD_API void        mod_cfg_set_enum(mod_cfg_handle_t cfg, int value);
MOD_API void        mod_cfg_set_string(mod_cfg_handle_t cfg, str_t value);
MOD_API void        mod_cfg_set_keybind(mod_cfg_handle_t cfg, keybind_t value);
MOD_API void        mod_cfg_set_color(mod_cfg_handle_t cfg, mod_color_t value);

/* Manager-owned arenas */
MOD_API arena_t *mod_get_perm(mod_handle_t mod);
MOD_API arena_t *mod_arena_create(mod_handle_t mod, uint64_t reserve_size, uint64_t commit_size);
MOD_API void     mod_arena_destroy(mod_handle_t mod, arena_t *arena);

/* Manager-owned engine listeners and native hooks */
MOD_API bool          mod_register_uobject_listener(mod_handle_t mod, uobject_listener_kind_t kind, uobject_on_notify_cb_t callback, void *user);
MOD_API void          mod_deregister_uobject_listener(mod_handle_t mod, uobject_listener_kind_t kind, uobject_on_notify_cb_t callback, void *user);
MOD_API sigscan_err_t mod_sigscan(sigscan_entry_t *entry);
MOD_API bool          mod_hook_create(mod_handle_t mod, void *target, void *detour, void **original);
MOD_API bool          mod_hook_enable(mod_handle_t mod, void *target);
MOD_API bool          mod_hook_disable(mod_handle_t mod, void *target);
MOD_API bool          mod_hook_remove(mod_handle_t mod, void *target);

/* Exported by each mod DLL */
MOD_EXTERN_C MOD_EXPORT mod_version_t    MOD_CALL mod_abi_version(void);
MOD_EXTERN_C MOD_EXPORT const mod_api_t *MOD_CALL mod_entry(void);

static inline bool
mod_abi_compatible(mod_version_t host, mod_version_t mod)
{
  return host.major == mod.major && host.minor == mod.minor;
}

MOD_EXTERN_C_END

#endif /* MOD_H */
