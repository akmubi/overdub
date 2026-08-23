#include "lua_runtime.h"

#include "config.h"
#include "log.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"
#include "lua/src/lualib.h"

#include <limits.h>
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

  size_t      len  = 0;
  const char *text = lua_tolstring(state, -1, &len);
  int         print_len = (int)MIN_VAL(len, (size_t)INT_MAX);
  CONSOLE_INFO("%.*s", print_len, text ? text : "");
  return 0;
}

static void
lua_runtime_set_global_nil(lua_State *state, const char *name)
{
  lua_pushnil(state);
  lua_setglobal(state, name);
}

static int
lua_runtime_open_libraries(lua_State *state)
{
  static const luaL_Reg libraries[] = {
    {LUA_GNAME, luaopen_base},
    {LUA_COLIBNAME, luaopen_coroutine},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {NULL, NULL},
  };

  for (const luaL_Reg *lib = libraries; lib->func; ++lib) {
    luaL_requiref(state, lib->name, lib->func, 1);
    lua_pop(state, 1);
  }

  lua_runtime_set_global_nil(state, "dofile");
  lua_runtime_set_global_nil(state, "loadfile");

  lua_pushcfunction(state, lua_runtime_print);
  lua_setglobal(state, "print");
  return 0;
}

bool
lua_runtime_init(lua_runtime_t *runtime)
{
  if (!runtime) {
    return false;
  }

  if (runtime->inited) {
    return true;
  }

  runtime->execution_limit_us = CONFIG_LUA_EXECUTION_LIMIT_US;
  runtime->memory_limit       = CONFIG_LUA_MEMORY_LIMIT;
  runtime->state              = lua_newstate(lua_runtime_alloc, runtime);
  if (!runtime->state) {
    LOG_ERROR("Failed to initialize Lua: could not allocate a state");
    return false;
  }

  *(lua_runtime_t **)lua_getextraspace(runtime->state) = runtime;

  lua_pushcfunction(runtime->state, lua_runtime_open_libraries);
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
  LOG_INFO("Lua %s initialized (execution limit: %llu us, memory limit: %llu bytes)",
           LUA_VERSION,
           (unsigned long long)runtime->execution_limit_us,
           (unsigned long long)runtime->memory_limit);
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
lua_runtime_load(lua_State *state, str_t source)
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

bool
lua_runtime_execute(lua_runtime_t *runtime, str_t source)
{
  if (!runtime || !runtime->inited || !runtime->state) {
    CONSOLE_ERROR("Lua runtime is unavailable");
    return false;
  }

  lua_State *state = runtime->state;
  lua_settop(state, 0);

  int status = lua_runtime_load(state, source);
  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    CONSOLE_ERROR("Lua syntax error: %s", error ? error : "unknown error");
    lua_settop(state, 0);
    return false;
  }

  lua_pushcfunction(state, lua_runtime_traceback);
  lua_insert(state, 1);

  runtime->execution_deadline_us = time_now_us() + runtime->execution_limit_us;
  lua_sethook(state, lua_runtime_limit_hook, LUA_MASKCOUNT, CONFIG_LUA_HOOK_INSTRUCTION_COUNT);

  status = lua_pcall(state, 0, LUA_MULTRET, 1);
  if (status == LUA_OK) {
    int result_count = lua_gettop(state) - 1; // exclude traceback handler
    lua_remove(state, 1);

    if (result_count > 0) {
      lua_pushcfunction(state, lua_runtime_print);
      lua_insert(state, 1);
      status = lua_pcall(state, result_count, 0, 0);
    }
  }

  lua_sethook(state, NULL, 0, 0);

  if (status != LUA_OK) {
    const char *error = lua_tostring(state, -1);
    CONSOLE_ERROR("Lua error: %s", error ? error : "unknown error");
    lua_settop(state, 0);
    return false;
  }

  lua_settop(state, 0);
  return true;
}
