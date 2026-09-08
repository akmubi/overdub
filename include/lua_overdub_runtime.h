#ifndef LUA_OVERDUB_RUNTIME_H
#define LUA_OVERDUB_RUNTIME_H

#include "input.h"
#include "str.h"
#include "unreal.h"

typedef struct mod_s         mod_t;
typedef struct mod_manager_s mod_manager_t;

struct nk_context;

void
lua_overdub_runtime_system_init(void);
bool
lua_overdub_runtime_start(mod_t *mod);
void
lua_overdub_runtime_stop(mod_t *mod);
bool
lua_overdub_runtime_is_initialized(mod_t *mod);
void
lua_overdub_runtime_tick(mod_t *mod, float delta_seconds);
bool
lua_overdub_runtime_input(mod_t *mod, input_event_t *event);
bool
lua_overdub_runtime_process_event_pre(mod_t *mod, uobject_t *object, ufunc_t *function, void *params);
void
lua_overdub_runtime_process_event_post(mod_t *mod, uobject_t *object, ufunc_t *function, void *params, bool consumed);
void
lua_overdub_runtime_post_load(mod_t *mod, uobject_t *object, bool after);
bool
lua_overdub_runtime_has_post_load_hooks(void);
bool
lua_overdub_runtime_command(mod_t *mod, str_t name, str_t args);
void
lua_overdub_runtime_notify_uobject_constructed(mod_manager_t *manager, uobject_t *object);
void
lua_overdub_runtime_notify_uobject_deleted(mod_manager_t *manager, uobject_t *object, int32_t idx);
int
lua_overdub_runtime_snapshot_commands(mod_t *mod, str_t *names, int capacity);
void
lua_overdub_runtime_draw_panel(mod_t *mod, struct nk_context *ctx);
bool
lua_overdub_runtime_draw_config(mod_t *mod, struct nk_context *ctx);
bool
lua_overdub_runtime_has_draw_config(mod_t *mod);

#endif /* LUA_OVERDUB_RUNTIME_H */
