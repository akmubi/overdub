#ifndef LUA_OVERDUB_UNREAL_H
#define LUA_OVERDUB_UNREAL_H

#include "arena.h"
#include "str.h"
#include "types.h"

typedef struct lua_State                    lua_State;
typedef struct lua_runtime_s                lua_runtime_t;
typedef struct lua_overdub_unreal_context_s lua_overdub_unreal_context_t;
typedef struct uobject_s                    uobject_t;
typedef struct ufunc_s                      ufunc_t;

lua_overdub_unreal_context_t *
lua_overdub_unreal_context_create(arena_t *arena, lua_runtime_t *runtime, bool *in_callback, str_t mod_id, uint32_t owner_thread_id);
void
lua_overdub_unreal_context_deactivate(lua_overdub_unreal_context_t *context);
void
lua_overdub_unreal_context_destroy(lua_overdub_unreal_context_t *context);
bool
lua_overdub_unreal_register(lua_State *state, lua_overdub_unreal_context_t *context);
void
lua_overdub_unreal_notify_constructed(uobject_t *object);
void
lua_overdub_unreal_notify_deleted(uobject_t *object, int32_t idx);
bool
lua_overdub_unreal_process_event_pre(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                                     bool *in_callback, uobject_t *object, ufunc_t *function, void *params);
void
lua_overdub_unreal_process_event_post(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                                      bool *in_callback, uobject_t *object, ufunc_t *function, void *params, bool consumed);
void
lua_overdub_unreal_post_load(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime, bool *in_callback, uobject_t *object, bool after);
bool
lua_overdub_unreal_has_post_load_hooks(void);
int
lua_overdub_unreal_post_load_hook_count(void);
uint64_t
lua_overdub_unreal_dispatch_pending(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                                    bool *in_callback, uint64_t budget_us);
int
lua_overdub_unreal_pending_count(lua_overdub_unreal_context_t *context);

#endif /* LUA_OVERDUB_UNREAL_H */
