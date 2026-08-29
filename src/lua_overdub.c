#include "lua_overdub.h"

#include "config.h"
#include "file.h"
#include "globals.h"
#include "ini.h"
#include "log.h"
#include "path.h"
#include "scratch.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"
#include "lua/src/lualib.h"

#include <string.h>
#include <windows.h>

#define LUA_OVERDUB_COMMAND_META "overdub.Command"

typedef struct lua_overdub_command_s lua_overdub_command_t;
struct lua_overdub_command_s {
  uint64_t id;
  size_t   name_len;
  int      callback_ref;
  char     name[CONFIG_LUA_OVERDUB_COMMAND_NAME_CAP];
  bool     active;
};

struct lua_overdub_command_context_s {
  lua_State            *state;
  lua_overdub_command_t commands[CONFIG_LUA_OVERDUB_MAX_COMMANDS];
  uint64_t              next_id;
};

typedef struct lua_overdub_command_handle_s lua_overdub_command_handle_t;
struct lua_overdub_command_handle_s {
  lua_overdub_context_t *context;
  uint64_t               id;
};

static char g_lua_overdub_context_key;

lua_overdub_context_t *
lua_overdub_context_get(lua_State *state)
{
  lua_pushlightuserdata(state, &g_lua_overdub_context_key);
  lua_rawget(state, LUA_REGISTRYINDEX);
  lua_overdub_context_t *context = (lua_overdub_context_t *)lua_touserdata(state, -1);
  lua_pop(state, 1);
  return context;
}

lua_overdub_context_t *
lua_overdub_context_check(lua_State *state)
{
  lua_overdub_context_t *context = lua_overdub_context_get(state);
  if (!context) {
    luaL_error(state, "Overdub runtime context is unavailable");
  }

  if (context->owner_thread_id == 0 || context->owner_thread_id != thread_current_id()) {
    luaL_error(state, "Overdub Lua state accessed outside its owning game thread");
  }
  return context;
}

static void
lua_overdub_push_str(lua_State *state, str_t value)
{
  lua_pushlstring(state, (const char *)(value.data ? value.data : (uint8_t *)""), (size_t)value.len);
}

static lua_overdub_command_t *
lua_overdub_command_find_id(lua_overdub_context_t *context, uint64_t id)
{
  if (!context || !context->commands || id == 0) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_COMMANDS; ++i) {
    lua_overdub_command_t *command = &context->commands->commands[i];
    if (command->active && command->id == id) {
      return command;
    }
  }

  return NULL;
}

static lua_overdub_command_t *
lua_overdub_command_find_name(lua_overdub_context_t *context, str_t name)
{
  if (!context || !context->commands || str_is_empty(name)) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_COMMANDS; ++i) {
    lua_overdub_command_t *command = &context->commands->commands[i];
    if (!command->active) {
      continue;
    }

    str_t registered_name = str_make(command->name, command->name_len);
    if (str_equal(registered_name, name, STR_CMP_FLAG_IGNORE_CASE)) {
      return command;
    }
  }

  return NULL;
}

static bool
lua_overdub_command_name_valid(const char *name, size_t len)
{
  if (!name || len == 0 || len >= CONFIG_LUA_OVERDUB_COMMAND_NAME_CAP || memchr(name, 0, len)) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    uint8_t c = (uint8_t)name[i];
    if (c <= ' ' || c == ':') {
      return false;
    }
  }

  return true;
}

bool
lua_overdub_command_find(lua_overdub_context_t *context, str_t name, uint64_t *out_id, int *out_ref)
{
  lua_overdub_command_t *command = lua_overdub_command_find_name(context, name);
  if (!command) {
    return false;
  }

  if (out_id) {
    *out_id = command->id;
  }

  if (out_ref) {
    *out_ref = command->callback_ref;
  }

  return true;
}

bool
lua_overdub_command_remove(lua_overdub_context_t *context, uint64_t id)
{
  lua_overdub_command_t *command = lua_overdub_command_find_id(context, id);
  if (!command) {
    return false;
  }

  luaL_unref(context->commands->state, LUA_REGISTRYINDEX, command->callback_ref);
  mem_zero(command, sizeof(*command));
  return true;
}

int
lua_overdub_command_snapshot(lua_overdub_context_t *context, str_t *names, int capacity)
{
  if (!context || !context->commands || !names || capacity <= 0) {
    return 0;
  }

  int count = 0;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_COMMANDS && count < capacity; ++i) {
    lua_overdub_command_t *command = &context->commands->commands[i];
    if (!command->active) {
      continue;
    }

    names[count] = str_make(command->name, command->name_len);
    count += 1;
  }

  return count;
}

void
lua_overdub_context_cleanup(lua_overdub_context_t *context)
{
  if (!context || !context->commands) {
    return;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_COMMANDS; ++i) {
    lua_overdub_command_t *command = &context->commands->commands[i];
    if (command->active) {
      luaL_unref(context->commands->state, LUA_REGISTRYINDEX, command->callback_ref);
    }
  }

  context->commands = NULL;
}

static lua_overdub_command_handle_t *
lua_overdub_command_handle_check(lua_State *state)
{
  lua_overdub_command_handle_t *handle  = luaL_checkudata(state, 1, LUA_OVERDUB_COMMAND_META);
  lua_overdub_context_t        *context = lua_overdub_context_check(state);
  if (!handle->context || handle->context != context) {
    luaL_error(state, "Overdub command handle is invalid");
  }

  return handle;
}

static int
lua_overdub_command_handle_remove(lua_State *state)
{
  lua_overdub_command_handle_t *handle = lua_overdub_command_handle_check(state);
  lua_pushboolean(state, lua_overdub_command_remove(handle->context, handle->id));
  return 1;
}

static int
lua_overdub_command_handle_is_active(lua_State *state)
{
  lua_overdub_command_handle_t *handle = lua_overdub_command_handle_check(state);
  lua_pushboolean(state, lua_overdub_command_find_id(handle->context, handle->id) != NULL);
  return 1;
}

static int
lua_overdub_command_handle_gc(lua_State *state)
{
  lua_overdub_command_handle_t *handle = luaL_checkudata(state, 1, LUA_OVERDUB_COMMAND_META);
  handle->context = NULL;
  handle->id      = 0;
  return 0;
}

static int
lua_overdub_register_command(lua_State *state)
{
  lua_overdub_context_t *context = lua_overdub_context_check(state);
  if (!context->commands) {
    return luaL_error(state, "Overdub console command context is unavailable");
  }

  size_t      name_len = 0;
  const char *name     = luaL_checklstring(state, 1, &name_len);
  if (!lua_overdub_command_name_valid(name, name_len)) {
    return luaL_argerror(state, 1, "command name must be non-empty, shorter than 128 bytes, and contain no spaces or ':'");
  }

  luaL_checktype(state, 2, LUA_TFUNCTION);
  str_t command_name = str_make((void *)name, name_len);
  if (lua_overdub_command_find_name(context, command_name)) {
    return luaL_error(state, "console command '%s' is already registered by this mod", name);
  }

  lua_overdub_command_t *command = NULL;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_COMMANDS; ++i) {
    if (!context->commands->commands[i].active) {
      command = &context->commands->commands[i];
      break;
    }
  }

  if (!command) {
    return luaL_error(state, "Overdub console command limit reached");
  }

  uint64_t id = context->commands->next_id;
  context->commands->next_id += 1;
  if (id == 0) {
    id = context->commands->next_id;
    context->commands->next_id += 1;
  }

  lua_pushvalue(state, 2);
  command->callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  command->id           = id;
  command->name_len     = name_len;
  command->active       = true;
  memcpy(command->name, name, name_len);
  command->name[name_len] = 0;

  lua_overdub_command_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_command_handle_t){
    .context = context,
    .id      = id,
  };
  luaL_setmetatable(state, LUA_OVERDUB_COMMAND_META);
  return 1;
}

static str_t
lua_overdub_check_path(lua_State *state, int index)
{
  size_t      len  = 0;
  const char *data = luaL_checklstring(state, index, &len);
  if (len == 0 || memchr(data, 0, len)) {
    luaL_argerror(state, index, "path must be non-empty and contain no null bytes");
    return STR_NULL;
  }

  return str_make((void *)data, len);
}

static int
lua_overdub_list_directory(lua_State *state)
{
  lua_overdub_context_check(state);
  str_t path = lua_overdub_check_path(state, 1);

  str_t pattern = STR_NULL;
  if (!lua_isnoneornil(state, 2)) {
    pattern = lua_overdub_check_path(state, 2);
  }

  if (!dir_exists(path)) {
    lua_pushnil(state);
    lua_pushliteral(state, "directory does not exist");
    return 2;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  dir_entry_list_t entries;
  if (str_is_empty(pattern)) {
    entries = dir_list(path, tmp.arena);
  } else {
    entries = dir_list_filter(path, pattern, tmp.arena);
  }

  lua_createtable(state, entries.count, 0);
  for (int i = 0; i < entries.count; ++i) {
    dir_entry_t *entry = &entries.items[i];
    lua_createtable(state, 0, 4);
    lua_overdub_push_str(state, entry->name);
    lua_setfield(state, -2, "Name");
    lua_overdub_push_str(state, entry->path);
    lua_setfield(state, -2, "Path");
    lua_pushboolean(state, entry->kind == DIR_ENTRY_DIR);
    lua_setfield(state, -2, "IsDirectory");
    lua_pushinteger(state, (lua_Integer)entry->size);
    lua_setfield(state, -2, "Size");
    lua_rawseti(state, -2, (lua_Integer)i + 1);
  }

  scratch_end(tmp);
  return 1;
}

static int
lua_overdub_read_ini(lua_State *state)
{
  lua_overdub_context_check(state);
  str_t path = lua_overdub_check_path(state, 1);
  if (!file_exists(path)) {
    lua_pushnil(state);
    lua_pushliteral(state, "INI file does not exist");
    return 2;
  }

  int         result = 1;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    uint64_t file_len = file_size(path);
    str_t    text     = file_read_all(path, tmp.arena);
    if (file_len == 0 || !str_is_empty(text)) {
      str_array_t        lines    = ini_split_lines(tmp.arena, text);
      ini_section_list_t sections = ini_parse_sections(tmp.arena, lines);
      lua_createtable(state, sections.count, 0);

      int section_index = 1;
      for (ini_section_t *section = sections.first; section; section = section->next) {
        lua_createtable(state, 0, 4);
        lua_overdub_push_str(state, section->name);
        lua_setfield(state, -2, "Name");

        if (str_is_empty(section->arg)) {
          lua_pushnil(state);
        } else {
          lua_overdub_push_str(state, section->arg);
        }
        lua_setfield(state, -2, "Argument");

        int line_capacity = (int)MIN_VAL(section->lines.count, (uint64_t)INT32_MAX);
        lua_createtable(state, line_capacity, 0);
        int line_index = 1;
        for (str_node_t *node = section->lines.first; node; node = node->next) {
          lua_overdub_push_str(state, node->str);
          lua_rawseti(state, -2, line_index);
          line_index += 1;
        }
        lua_setfield(state, -2, "Lines");

        lua_newtable(state);
        for (str_node_t *node = section->lines.first; node; node = node->next) {
          str_t key   = STR_NULL;
          str_t value = STR_NULL;
          if (!ini_parse_kv(node->str, &key, &value)) {
            continue;
          }

          lua_overdub_push_str(state, key);
          lua_overdub_push_str(state, value);
          lua_rawset(state, -3);
        }
        lua_setfield(state, -2, "Values");

        lua_rawseti(state, -2, section_index);
        section_index += 1;
      }
    } else {
      lua_pushnil(state);
      lua_pushliteral(state, "could not read INI file");
      result = 2;
    }
  }
  scratch_end(tmp);
  return result;
}

static int
lua_overdub_frame(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushinteger(state, (lua_Integer)globals.frame_counter);
  return 1;
}

static int
lua_overdub_now_us(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushinteger(state, (lua_Integer)time_now_us());
  return 1;
}

static int
lua_overdub_log(lua_State *state)
{
  lua_overdub_context_t *context = lua_overdub_context_check(state);
  int                    level   = (int)lua_tointeger(state, lua_upvalueindex(1));
  int                    count   = lua_gettop(state);
  luaL_argcheck(state, count > 0, 1, "message or format string expected");

  lua_getglobal(state, LUA_STRLIBNAME);
  lua_getfield(state, -1, "format");
  lua_remove(state, -2);
  lua_insert(state, 1);
  lua_call(state, count, 1);

  size_t      message_len = 0;
  const char *message     = luaL_checklstring(state, -1, &message_len);
  int         log_len     = (int)MIN_VAL(message_len, (size_t)INT32_MAX);

  level = CLAMP(level, LOG_LEVEL_DEBUG, LOG_LEVEL_ERROR);
  log_emit((log_level_t)level, LOG_SINK_DEFAULT, "[%.*s] %.*s", STR_ARG(context->mod_id), log_len, message);
  return 0;
}

static void
lua_overdub_set_string_field(lua_State *state, const char *name, str_t value)
{
  lua_overdub_push_str(state, value);
  lua_setfield(state, -2, name);
}

static void
lua_overdub_set_log_function(lua_State *state, const char *name, log_level_t level)
{
  lua_pushinteger(state, (lua_Integer)level);
  lua_pushcclosure(state, lua_overdub_log, 1);
  lua_setfield(state, -2, name);
}

static int
luaopen_overdub(lua_State *state)
{
  lua_overdub_context_t *context = lua_overdub_context_check(state);

  lua_newtable(state);
  lua_overdub_set_string_field(state, "Id", context->mod_id);
  lua_overdub_set_string_field(state, "Name", context->mod_name);
  lua_overdub_set_string_field(state, "GameDir", context->game_dir);
  lua_overdub_set_string_field(state, "RootDir", context->root_dir);
  lua_overdub_set_string_field(state, "ModDir", context->mod_dir);

  lua_pushcfunction(state, lua_overdub_frame);
  lua_setfield(state, -2, "Frame");
  lua_pushcfunction(state, lua_overdub_now_us);
  lua_setfield(state, -2, "NowUs");
  lua_pushcfunction(state, lua_overdub_register_command);
  lua_setfield(state, -2, "RegisterCommand");
  lua_pushcfunction(state, lua_overdub_list_directory);
  lua_setfield(state, -2, "ListDirectory");
  lua_pushcfunction(state, lua_overdub_read_ini);
  lua_setfield(state, -2, "ReadIni");

  lua_newtable(state);
  lua_overdub_set_log_function(state, "Debug", LOG_LEVEL_DEBUG);
  lua_overdub_set_log_function(state, "Info",  LOG_LEVEL_INFO);
  lua_overdub_set_log_function(state, "Warn",  LOG_LEVEL_WARN);
  lua_overdub_set_log_function(state, "Error", LOG_LEVEL_ERROR);
  lua_setfield(state, -2, "Log");
  return 1;
}

static bool
lua_overdub_prepend_package_field(lua_State *state, const char *field, str_t first_dir, str_t second_dir,
                                  const char *first_suffix, const char *second_suffix)
{
  lua_getglobal(state, LUA_LOADLIBNAME);
  if (!lua_istable(state, -1)) {
    lua_pop(state, 1);
    return false;
  }

  lua_getfield(state, -1, field);
  size_t      old_len  = 0;
  const char *old_path = lua_tolstring(state, -1, &old_len);

  luaL_Buffer buffer;
  luaL_buffinit(state, &buffer);
  if (!str_is_empty(first_dir)) {
    luaL_addlstring(&buffer, (const char *)first_dir.data, (size_t)first_dir.len);
    luaL_addstring(&buffer, first_suffix);

    if (second_suffix) {
      luaL_addlstring(&buffer, (const char *)first_dir.data, (size_t)first_dir.len);
      luaL_addstring(&buffer, second_suffix);
    }
  }

  if (!str_is_empty(second_dir) && !path_equal(first_dir, second_dir)) {
    luaL_addlstring(&buffer, (const char *)second_dir.data, (size_t)second_dir.len);
    luaL_addstring(&buffer, first_suffix);

    if (second_suffix) {
      luaL_addlstring(&buffer, (const char *)second_dir.data, (size_t)second_dir.len);
      luaL_addstring(&buffer, second_suffix);
    }
  }

  if (old_path && old_len > 0) {
    luaL_addlstring(&buffer, old_path, old_len);
  }
  luaL_pushresult(&buffer);

  lua_setfield(state, -3, field);
  lua_pop(state, 2); // previous value and package table
  return true;
}

bool
lua_overdub_preload_module(lua_State *state, const char *name, int (*open_fn)(lua_State *state))
{
  if (!state || !name || !open_fn) {
    return false;
  }

  luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  lua_pushcfunction(state, open_fn);
  lua_setfield(state, -2, name);
  lua_pop(state, 1);
  return true;
}

bool
lua_overdub_configure_package_paths(lua_State *state, str_t entry_path, str_t mod_dir)
{
  if (!state) {
    return false;
  }

  str_t entry_dir = path_dir(entry_path);
  return lua_overdub_prepend_package_field(state, "path",  entry_dir, mod_dir, "/?.lua;", "/?/init.lua;") &&
         lua_overdub_prepend_package_field(state, "cpath", entry_dir, mod_dir, "/?.dll;", NULL);
}

bool
lua_overdub_register(lua_State *state, lua_overdub_context_t *context)
{
  if (!state || !context || !context->arena || !context->arena->backing || context->owner_thread_id == 0 || context->owner_thread_id != thread_current_id()) {
    return false;
  }

  lua_pushlightuserdata(state, &g_lua_overdub_context_key);
  lua_pushlightuserdata(state, context);
  lua_rawset(state, LUA_REGISTRYINDEX);

  context->commands = ARENA_PUSH_ZERO(context->arena, lua_overdub_command_context_t);
  if (!context->commands) {
    return false;
  }
  context->commands->state   = state;
  context->commands->next_id = 1;

  if (luaL_newmetatable(state, LUA_OVERDUB_COMMAND_META)) {
    static const luaL_Reg methods[] = {
      {"Remove",   lua_overdub_command_handle_remove   },
      {"IsActive", lua_overdub_command_handle_is_active},
      {NULL,       NULL                                },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, lua_overdub_command_handle_gc);
    lua_setfield(state, -2, "__gc");
    lua_pushliteral(state, "Overdub console command");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  if (!lua_overdub_configure_package_paths(state, context->entry_path, context->mod_dir)) {
    lua_overdub_context_cleanup(context);
    return false;
  }

  bool registered = lua_overdub_preload_module(state, "overdub", luaopen_overdub);
  if (!registered) {
    lua_overdub_context_cleanup(context);
  }

  return registered;
}
