#ifndef LUA_OVERDUB_TASK_H
#define LUA_OVERDUB_TASK_H

#include "lua_runtime.h"
#include "str.h"
#include "types.h"

typedef struct lua_overdub_task_context_s   lua_overdub_task_context_t;
typedef struct lua_overdub_worker_context_s lua_overdub_worker_context_t;

lua_overdub_task_context_t *
lua_overdub_task_context_create(arena_t *arena, lua_runtime_t *runtime, lua_overdub_worker_context_t *worker, str_t mod_id);
void
lua_overdub_task_context_destroy(lua_overdub_task_context_t *context);
bool
lua_overdub_task_register(lua_State *state, lua_overdub_task_context_t *context);
uint64_t
lua_overdub_task_dispatch(lua_overdub_task_context_t *context, uint64_t budget_us);
int
lua_overdub_task_active_count(lua_overdub_task_context_t *context);
uint64_t
lua_overdub_task_dispatch_worker_completions(lua_overdub_task_context_t *context, bool *in_callback, uint64_t budget_us);
int
lua_overdub_task_future_count(lua_overdub_task_context_t *context);

#endif /* LUA_OVERDUB_TASK_H */
