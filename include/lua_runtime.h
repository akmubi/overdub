#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H

#include "str.h"
#include "types.h"

typedef struct lua_State     lua_State;
typedef struct lua_runtime_s lua_runtime_t;
struct lua_runtime_s {
  lua_State *state;
  uint64_t   execution_deadline_us;
  uint64_t   execution_limit_us;
  uint64_t   memory_used;
  uint64_t   memory_limit;
  bool       inited;
};

bool
lua_runtime_init(lua_runtime_t *runtime);
void
lua_runtime_deinit(lua_runtime_t *runtime);
bool
lua_runtime_execute(lua_runtime_t *runtime, str_t source);

#endif /* LUA_RUNTIME_H */
