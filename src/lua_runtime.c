#include "lua_runtime.h"

#include "config.h"
#include "file.h"
#include "log.h"
#include "scratch.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"
#include "lua/src/lualib.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void *
lua_runtime_alloc(void *user, void *ptr, size_t old_size, size_t new_size)
{
  lua_runtime_t *runtime = user;

  if (new_size == 0) {
    free(ptr);
    runtime->memory_used -= MIN_VAL(runtime->memory_used, (uint64_t)old_size);
    return NULL;
  }

  if (!ptr) {
    old_size = 0;
  }

  if (new_size > old_size) {
    uint64_t growth = (uint64_t)(new_size - old_size);
    if (growth > runtime->memory_limit - MIN_VAL(runtime->memory_used, runtime->memory_limit)) {
      return NULL;
    }
  }

  void *new_ptr = realloc(ptr, new_size);
  if (!new_ptr) {
    return NULL;
  }

  runtime->memory_used -= MIN_VAL(runtime->memory_used, (uint64_t)old_size);
  runtime->memory_used += (uint64_t)new_size;
  return new_ptr;
}

static lua_runtime_t *
lua_runtime_from_state(lua_State *state)
{
  return *(lua_runtime_t **)lua_getextraspace(state);
}

static uint64_t
lua_runtime_error_hash(const char *error)
{
  uint64_t hash = 14695981039346656037ULL;
  for (const uint8_t *at = (const uint8_t *)(error ? error : "unknown error"); *at; ++at) {
    hash ^= *at;
    hash *= 1099511628211ULL;
  }
  return hash ? hash : 1;
}

static void
lua_runtime_limit_hook(lua_State *state, lua_Debug *debug)
{
  (void)debug;

  lua_runtime_t *runtime = lua_runtime_from_state(state);
  if (runtime && time_now_us() >= runtime->execution_deadline_us) {
    luaL_error(state, "execution time limit exceeded");
  }
}

static int
lua_runtime_traceback(lua_State *state)
{
  const char *message = lua_tostring(state, 1);
  if (message) {
    luaL_traceback(state, state, message, 1);
  } else if (!lua_isnoneornil(state, 1)) {
    lua_pushliteral(state, "(error object is not a string)");
  }
  return 1;
}

static int
lua_runtime_print(lua_State *state)
{
  int         arg_count = lua_gettop(state);
  luaL_Buffer buffer;
  luaL_buffinit(state, &buffer);

  for (int i = 1; i <= arg_count; ++i) {
    if (i > 1) {
      luaL_addchar(&buffer, '\t');
    }

    size_t      len  = 0;
    const char *text = luaL_tolstring(state, i, &len);
    luaL_addlstring(&buffer, text, len);
    lua_pop(state, 1);
  }

  luaL_pushresult(&buffer);

  size_t         len       = 0;
  const char    *text      = lua_tolstring(state, -1, &len);
  int            print_len = (int)MIN_VAL(len, (size_t)INT_MAX);
  lua_runtime_t *runtime   = lua_runtime_from_state(state);
  if (runtime && runtime->persistent_output) {
    LOG_INFO("%.*s", print_len, text ? text : "");
  } else {
    CONSOLE_INFO("%.*s", print_len, text ? text : "");
  }
  return 0;
}

static void
lua_runtime_set_global_nil(lua_State *state, const char *name)
{
  lua_pushnil(state);
  lua_setglobal(state, name);
}

static int
lua_runtime_open_libs(lua_State *state)
{
  static const luaL_Reg libs[] = {
    {LUA_GNAME,       luaopen_base     },
    {LUA_COLIBNAME,   luaopen_coroutine},
    {LUA_TABLIBNAME,  luaopen_table    },
    {LUA_STRLIBNAME,  luaopen_string   },
    {LUA_MATHLIBNAME, luaopen_math     },
    {LUA_UTF8LIBNAME, luaopen_utf8     },
    {NULL,            NULL             },
  };

  for (const luaL_Reg *lib = libs; lib->func; ++lib) {
    luaL_requiref(state, lib->name, lib->func, 1);
    lua_pop(state, 1);
  }

  lua_runtime_set_global_nil(state, "dofile");
  lua_runtime_set_global_nil(state, "loadfile");

  lua_pushcfunction(state, lua_runtime_print);
  lua_setglobal(state, "print");
  return 0;
}

static int
lua_runtime_open_mod_libs(lua_State *state)
{
  static const luaL_Reg libraries[] = {
    {LUA_LOADLIBNAME, luaopen_package},
    {LUA_IOLIBNAME,   luaopen_io     },
    {LUA_OSLIBNAME,   luaopen_os     },
    {NULL,            NULL           },
  };

  for (const luaL_Reg *lib = libraries; lib->func; ++lib) {
    luaL_requiref(state, lib->name, lib->func, 1);
    lua_pop(state, 1);
  }
  return 0;
}

bool
lua_runtime_init_with_execution_limit(lua_runtime_t *runtime, uint64_t execution_limit_us)
{
  if (!runtime) {
    return false;
  }

  if (runtime->inited) {
    return true;
  }

  runtime->execution_limit_us = execution_limit_us;
  runtime->memory_limit       = CONFIG_LUA_MEMORY_LIMIT;
  runtime->state              = lua_newstate(lua_runtime_alloc, runtime);
  if (!runtime->state) {
    LOG_ERROR("Failed to initialize Lua: could not allocate a state");
    return false;
  }

  *(lua_runtime_t **)lua_getextraspace(runtime->state) = runtime;

  lua_pushcfunction(runtime->state, lua_runtime_open_libs);
  int status = lua_pcall(runtime->state, 0, 0, 0);
  if (status != LUA_OK) {
    const char *error = lua_tostring(runtime->state, -1);
    LOG_ERROR("Failed to initialize Lua: %s", error ? error : "unknown error");
    lua_close(runtime->state);
    runtime->state       = NULL;
    runtime->memory_used = 0;
    return false;
  }

  runtime->inited = true;
  if (runtime->execution_limit_us > 0) {
    LOG_INFO("Lua %s initialized (execution limit: %llu us, memory limit: %llu bytes)",
             LUA_VERSION,
             (unsigned long long)runtime->execution_limit_us,
             (unsigned long long)runtime->memory_limit);
  } else {
    LOG_INFO("Lua %s initialized (execution limit: disabled, memory limit: %llu bytes)",
             LUA_VERSION,
             (unsigned long long)runtime->memory_limit);
  }
  return true;
}

bool
lua_runtime_init(lua_runtime_t *runtime)
{
  return lua_runtime_init_with_execution_limit(runtime, CONFIG_LUA_EXECUTION_LIMIT_US);
}

bool
lua_runtime_enable_mod_libs(lua_runtime_t *runtime)
{
  if (!runtime || !runtime->inited || !runtime->state) {
    return false;
  }

  lua_pushcfunction(runtime->state, lua_runtime_open_mod_libs);
  int status = lua_pcall(runtime->state, 0, 0, 0);
  if (status != LUA_OK) {
    const char *error = lua_tostring(runtime->state, -1);
    LOG_ERROR("Failed to enable Lua mod libraries: %s", error ? error : "unknown error");
    lua_pop(runtime->state, 1);
    return false;
  }
  return true;
}

void
lua_runtime_deinit(lua_runtime_t *runtime)
{
  if (!runtime || !runtime->state) {
    return;
  }

  lua_close(runtime->state);
  runtime->state       = NULL;
  runtime->memory_used = 0;
  runtime->inited      = false;
}

static int
lua_runtime_load_console(lua_State *state, str_t source)
{
  static const char expression_prefix[] = "return ";

  luaL_Buffer buffer;
  luaL_buffinit(state, &buffer);
  luaL_addlstring(&buffer, expression_prefix, sizeof(expression_prefix) - 1);
  luaL_addlstring(&buffer, (const char *)source.data, source.len);
  luaL_pushresult(&buffer);

  size_t      expression_len = 0;
  const char *expression     = lua_tolstring(state, -1, &expression_len);
  int status = luaL_loadbufferx(state, expression, expression_len, "=console", "t");
  lua_remove(state, -2); // expression source string
  if (status == LUA_OK) {
    return status;
  }

  lua_pop(state, 1); // expression syntax error
  return luaL_loadbufferx(state, (const char *)source.data, source.len, "=console", "t");
}

static bool
lua_runtime_call_loaded(lua_runtime_t *runtime, bool print_results, bool persistent_errors)
{
  lua_State *state        = runtime->state;
  int        function_idx = lua_gettop(state);

  lua_pushcfunction(state, lua_runtime_traceback);
  lua_insert(state, function_idx);

  if (runtime->execution_limit_us > 0) {
    runtime->execution_deadline_us = time_now_us() + runtime->execution_limit_us;
    lua_sethook(state, lua_runtime_limit_hook, LUA_MASKCOUNT, CONFIG_LUA_HOOK_INSTRUCTION_COUNT);
  } else {
    lua_sethook(state, NULL, 0, 0);
  }

  int status = lua_pcall(state, 0, LUA_MULTRET, function_idx);
  if (status == LUA_OK) {
    int result_count = lua_gettop(state) - function_idx;
    lua_remove(state, function_idx);

    if (print_results && result_count > 0) {
      lua_pushcfunction(state, lua_runtime_print);
      lua_insert(state, 1);
      status = lua_pcall(state, result_count, 0, 0);
    }
  }

  lua_sethook(state, NULL, 0, 0);

  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    if (persistent_errors) {
      LOG_ERROR("Lua error: %s", error ? error : "unknown error");
    } else {
      CONSOLE_ERROR("Lua error: %s", error ? error : "unknown error");
    }
    lua_settop(state, 0);
    return false;
  }

  lua_settop(state, 0);
  return true;
}

static bool
lua_runtime_call_loaded_results(lua_runtime_t *runtime, int result_count, bool persistent_errors)
{
  if (!runtime || !runtime->state || result_count < 0) {
    return false;
  }

  lua_State *state        = runtime->state;
  int        function_idx = lua_gettop(state);
  if (!lua_isfunction(state, function_idx)) {
    lua_settop(state, 0);
    return false;
  }

  lua_pushcfunction(state, lua_runtime_traceback);
  lua_insert(state, function_idx);

  if (runtime->execution_limit_us > 0) {
    runtime->execution_deadline_us = time_now_us() + runtime->execution_limit_us;
    lua_sethook(state, lua_runtime_limit_hook, LUA_MASKCOUNT, CONFIG_LUA_HOOK_INSTRUCTION_COUNT);
  } else {
    lua_sethook(state, NULL, 0, 0);
  }

  int status = lua_pcall(state, 0, result_count, function_idx);
  lua_sethook(state, NULL, 0, 0);

  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    if (persistent_errors) {
      LOG_ERROR("Lua error: %s", error ? error : "unknown error");
    } else {
      CONSOLE_ERROR("Lua error: %s", error ? error : "unknown error");
    }
    lua_settop(state, 0);
    return false;
  }

  lua_remove(state, function_idx); // traceback function
  return true;
}

bool
lua_runtime_execute(lua_runtime_t *runtime, str_t source)
{
  if (!runtime || !runtime->inited || !runtime->state) {
    CONSOLE_ERROR("Lua runtime is unavailable");
    return false;
  }

  lua_State *state = runtime->state;
  lua_settop(state, 0);

  int status = lua_runtime_load_console(state, source);
  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    CONSOLE_ERROR("Lua syntax error: %s", error ? error : "unknown error");
    lua_settop(state, 0);
    return false;
  }

  return lua_runtime_call_loaded(runtime, true, false);
}

bool
lua_runtime_execute_file(lua_runtime_t *runtime, str_t path)
{
  if (!runtime || !runtime->inited || !runtime->state || !file_exists(path)) {
    return false;
  }

  lua_State  *state  = runtime->state;
  int         status = 0;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str_t source = file_read_all(path, tmp.arena);
    lua_settop(state, 0);

    lua_pushliteral(state, "@");
    lua_pushlstring(state, (const char *)path.data, (size_t)path.len);
    lua_concat(state, 2);

    const char *chunk_name  = lua_tostring(state, -1);
    const char *source_data = source.data ? (const char *)source.data : "";

    status = luaL_loadbufferx(state, source_data, (size_t)source.len, chunk_name, "t");
    lua_remove(state, -2); // chunk name
  }
  scratch_end(tmp);

  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    LOG_ERROR("Lua syntax error: %s", error ? error : "unknown error");
    lua_settop(state, 0);
    return false;
  }

  return lua_runtime_call_loaded(runtime, false, true);
}

bool
lua_runtime_execute_file_results(lua_runtime_t *runtime, str_t path, int result_count)
{
  if (!runtime || !runtime->inited || !runtime->state || result_count < 0 || !file_exists(path)) {
    return false;
  }

  lua_State  *state  = runtime->state;
  int         status = 0;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str_t source = file_read_all(path, tmp.arena);
    lua_settop(state, 0);

    lua_pushliteral(state, "@");
    lua_pushlstring(state, (const char *)path.data, (size_t)path.len);
    lua_concat(state, 2);

    const char *chunk_name  = lua_tostring(state, -1);
    const char *source_data = source.data ? (const char *)source.data : "";

    status = luaL_loadbufferx(state, source_data, (size_t)source.len, chunk_name, "t");
    lua_remove(state, -2); // chunk name
  }
  scratch_end(tmp);

  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    LOG_ERROR("Lua syntax error: %s", error ? error : "unknown error");
    lua_settop(state, 0);
    return false;
  }

  return lua_runtime_call_loaded_results(runtime, result_count, true);
}

bool
lua_runtime_call_ref(lua_runtime_t *runtime, int callback_ref, int arg_count, int result_count)
{
  return lua_runtime_call_ref_ex(runtime, callback_ref, arg_count, result_count, true, 0, NULL);
}

bool
lua_runtime_pcall(lua_runtime_t *runtime, int arg_count, int result_count, char *error, size_t error_cap)
{
  if (error && error_cap > 0) {
    error[0] = 0;
  }

  if (!runtime || !runtime->inited || !runtime->state || arg_count < 0 || result_count < 0) {
    return false;
  }

  lua_State *state        = runtime->state;
  int        top          = lua_gettop(state);
  int        function_idx = top - arg_count;
  if (function_idx < 1 || !lua_isfunction(state, function_idx)) {
    return false;
  }

  lua_pushcfunction(state, lua_runtime_traceback);
  lua_insert(state, function_idx);

  lua_Hook previous_hook       = lua_gethook(state);
  int      previous_hook_mask  = lua_gethookmask(state);
  int      previous_hook_count = lua_gethookcount(state);
  uint64_t previous_deadline   = runtime->execution_deadline_us;

  if (runtime->execution_limit_us > 0) {
    runtime->execution_deadline_us = time_now_us() + runtime->execution_limit_us;
    lua_sethook(state, lua_runtime_limit_hook, LUA_MASKCOUNT, CONFIG_LUA_HOOK_INSTRUCTION_COUNT);
  } else {
    lua_sethook(state, NULL, 0, 0);
  }

  int status = lua_pcall(state, arg_count, result_count, function_idx);
  runtime->execution_deadline_us = previous_deadline;
  lua_sethook(state, previous_hook, previous_hook_mask, previous_hook_count);

  if (status != LUA_OK) {
    const char *message = lua_tostring(state, -1);
    if (error && error_cap > 0) {
      snprintf(error, error_cap, "%s", message ? message : "unknown Lua error");
      error[error_cap - 1] = 0;
    }

    lua_settop(state, function_idx - 1);
    return false;
  }

  lua_remove(state, function_idx); /* traceback function */
  return true;
}

bool
lua_runtime_call_ref_ex(lua_runtime_t *runtime, int callback_ref, int arg_count, int result_count, bool log_error,
                        uint64_t suppressed_error_hash, uint64_t *out_error_hash)
{
  if (out_error_hash) {
    *out_error_hash = 0;
  }

  if (!runtime || !runtime->inited || !runtime->state || callback_ref == LUA_NOREF || callback_ref == LUA_REFNIL || arg_count < 0) {
    return false;
  }

  lua_State *state = runtime->state;
  int        top   = lua_gettop(state);
  if (arg_count > top) {
    return false;
  }

  int base = top - arg_count + 1;
  lua_pushcfunction(state, lua_runtime_traceback);
  lua_insert(state, base);
  lua_rawgeti(state, LUA_REGISTRYINDEX, callback_ref);
  lua_insert(state, base + 1);

  lua_Hook previous_hook       = lua_gethook(state);
  int      previous_hook_mask  = lua_gethookmask(state);
  int      previous_hook_count = lua_gethookcount(state);
  uint64_t previous_deadline   = runtime->execution_deadline_us;

  if (runtime->execution_limit_us > 0) {
    runtime->execution_deadline_us = time_now_us() + runtime->execution_limit_us;
    lua_sethook(state, lua_runtime_limit_hook, LUA_MASKCOUNT, CONFIG_LUA_HOOK_INSTRUCTION_COUNT);
  } else {
    lua_sethook(state, NULL, 0, 0);
  }

  int status = lua_pcall(state, arg_count, result_count, base);
  runtime->execution_deadline_us = previous_deadline;
  lua_sethook(state, previous_hook, previous_hook_mask, previous_hook_count);

  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    uint64_t    error_hash = lua_runtime_error_hash(error);
    if (out_error_hash) {
      *out_error_hash = error_hash;
    }

    if (log_error && error_hash != suppressed_error_hash) {
      LOG_ERROR("Lua error: %s", error ? error : "unknown error");
    }

    lua_settop(state, base - 1);
    return false;
  }

  lua_remove(state, base); // traceback function
  return true;
}

int
lua_runtime_resume(lua_runtime_t *runtime, lua_State *thread, int arg_count, int *result_count)
{
  if (result_count) {
    *result_count = 0;
  }

  if (!runtime || !runtime->inited || !runtime->state || !thread || arg_count < 0) {
    return LUA_ERRRUN;
  }

  lua_Hook previous_hook       = lua_gethook(thread);
  int      previous_hook_mask  = lua_gethookmask(thread);
  int      previous_hook_count = lua_gethookcount(thread);
  uint64_t previous_deadline   = runtime->execution_deadline_us;

  if (runtime->execution_limit_us > 0) {
    runtime->execution_deadline_us = time_now_us() + runtime->execution_limit_us;
    lua_sethook(thread, lua_runtime_limit_hook, LUA_MASKCOUNT, CONFIG_LUA_HOOK_INSTRUCTION_COUNT);
  } else {
    lua_sethook(thread, NULL, 0, 0);
  }

  int yielded_results = 0;
  int status = lua_resume(thread, runtime->state, arg_count, &yielded_results);

  runtime->execution_deadline_us = previous_deadline;
  lua_sethook(thread, previous_hook, previous_hook_mask, previous_hook_count);

  if (result_count) {
    *result_count = yielded_results;
  }

  if (status != LUA_OK && status != LUA_YIELD) {
    const char *error = lua_tostring(thread, -1);
    luaL_traceback(runtime->state, thread, error ? error : "unknown coroutine error", 1);

    const char *traceback = lua_tostring(runtime->state, -1);
    LOG_ERROR("Lua task error: %s", traceback ? traceback : (error ? error : "unknown error"));
    lua_pop(runtime->state, 1);
    lua_settop(thread, 0);
  }
  return status;
}
