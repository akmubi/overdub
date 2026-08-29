#ifndef LUA_UE4SS_RUNTIME_H
#define LUA_UE4SS_RUNTIME_H

#include "input.h"
#include "str.h"
#include "unreal.h"

typedef struct mod_s         mod_t;
typedef struct mod_manager_s mod_manager_t;

void
lua_ue4ss_runtime_system_init(void);
bool
lua_ue4ss_runtime_start(mod_t *mod);
void
lua_ue4ss_runtime_stop(mod_t *mod);
bool
lua_ue4ss_runtime_is_initialized(mod_t *mod);
void
lua_ue4ss_runtime_tick(mod_t *mod);
void
lua_ue4ss_runtime_queue_input(mod_t *mod, input_event_t *event);
bool
lua_ue4ss_runtime_process_event_pre(mod_t *mod, uobject_t *object, ufunc_t *function, void *params);
void
lua_ue4ss_runtime_process_event_post(mod_t *mod, uobject_t *object, ufunc_t *function, void *params, bool consumed);
bool
lua_ue4ss_runtime_queue_command(mod_t *mod, str_t name, str_t args);
void
lua_ue4ss_runtime_notify_uobject_constructed(mod_manager_t *manager, uobject_t *object);
int
lua_ue4ss_runtime_snapshot_commands(mod_t *mod, str_t *names, int capacity);

#endif /* LUA_UE4SS_RUNTIME_H */
