#ifndef LUA_OVERDUB_H
#define LUA_OVERDUB_H

#include "str.h"
#include "types.h"

typedef struct lua_State lua_State;

typedef struct lua_overdub_command_context_s lua_overdub_command_context_t;
typedef struct lua_overdub_context_s lua_overdub_context_t;
struct lua_overdub_context_s {
  arena_t                       *arena;
  str_t                          mod_id;
  str_t                          mod_name;
  str_t                          game_dir;
  str_t                          root_dir;
  str_t                          mod_dir;
  str_t                          entry_path;
  lua_overdub_command_context_t *commands;
  uint32_t                       owner_thread_id;
};

bool
lua_overdub_register(lua_State *state, lua_overdub_context_t *context);
lua_overdub_context_t *
lua_overdub_context_get(lua_State *state);
lua_overdub_context_t *
lua_overdub_context_check(lua_State *state);
bool
lua_overdub_preload_module(lua_State *state, const char *name, int (*open_fn)(lua_State *state));
bool
lua_overdub_configure_package_paths(lua_State *state, str_t entry_path, str_t mod_dir);
void
lua_overdub_context_cleanup(lua_overdub_context_t *context);
bool
lua_overdub_command_find(lua_overdub_context_t *context, str_t name, uint64_t *out_id, int *out_ref);
bool
lua_overdub_command_remove(lua_overdub_context_t *context, uint64_t id);
int
lua_overdub_command_snapshot(lua_overdub_context_t *context, str_t *names, int capacity);

#endif /* LUA_OVERDUB_H */
