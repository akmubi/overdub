#ifndef LUA_OVERDUB_UI_H
#define LUA_OVERDUB_UI_H

#include "lua_runtime.h"
#include "str.h"
#include "types.h"

typedef struct lua_State lua_State;

struct nk_context;

typedef struct lua_overdub_ui_context_s lua_overdub_ui_context_t;

lua_overdub_ui_context_t *
lua_overdub_ui_context_create(arena_t *arena, lua_runtime_t *runtime, str_t mod_id, uint32_t owner_thread_id);
void
lua_overdub_ui_context_destroy(lua_overdub_ui_context_t *context);
bool
lua_overdub_ui_register(lua_State *state, lua_overdub_ui_context_t *context);
uint64_t
lua_overdub_ui_draw_windows(lua_overdub_ui_context_t *ui, struct nk_context *nk, bool *in_cb, unsigned int vw, unsigned int vh);
bool
lua_overdub_ui_begin_callback(lua_overdub_ui_context_t *ui, struct nk_context *nk, unsigned int vw, unsigned int vh);
void
lua_overdub_ui_end_callback(lua_overdub_ui_context_t *ui);

#endif /* LUA_OVERDUB_UI_H */
