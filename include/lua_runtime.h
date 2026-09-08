#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H

#include "str.h"
#include "types.h"

typedef struct lua_State lua_State;

typedef struct lua_runtime_s lua_runtime_t;
struct lua_runtime_s {
  lua_State *state;
  uint64_t   execution_deadline_us;
  uint64_t   execution_limit_us;
  uint64_t   memory_used;
  uint64_t   memory_limit;
  bool       persistent_output;
  bool       inited;
};

bool
lua_runtime_init(lua_runtime_t *runtime);
bool
lua_runtime_init_with_execution_limit(lua_runtime_t *runtime, uint64_t execution_limit_us);
bool
lua_runtime_enable_mod_libs(lua_runtime_t *runtime);
void
lua_runtime_deinit(lua_runtime_t *runtime);
bool
lua_runtime_execute(lua_runtime_t *runtime, str_t source);
bool
lua_runtime_execute_file(lua_runtime_t *runtime, str_t path);
bool
lua_runtime_execute_file_results(lua_runtime_t *runtime, str_t path, int result_count);
bool
lua_runtime_call_ref(lua_runtime_t *runtime, int callback_ref, int arg_count, int result_count);
bool
lua_runtime_call_ref_ex(lua_runtime_t *runtime, int callback_ref, int arg_count, int result_count, bool log_error,
                        uint64_t suppressed_error_hash, uint64_t *out_error_hash);
int
lua_runtime_resume(lua_runtime_t *runtime, lua_State *thread, int arg_count, int *result_count);
bool
lua_runtime_pcall(lua_runtime_t *runtime, int arg_count, int result_count, char *error, size_t error_cap);

#endif /* LUA_RUNTIME_H */
