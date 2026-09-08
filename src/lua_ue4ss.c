#include "lua_ue4ss.h"

#include "config.h"
#include "file.h"
#include "globals.h"
#include "log.h"
#include "path.h"
#include "scratch.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <limits.h>
#include <stdlib.h>

#define LUA_UE4SS_DIRECTORY_MAX_DEPTH (6)
#define LUA_UE4SS_DIRECTORY_MAX_NODES (2048)

static const char g_lua_ue4ss_context_registry_key = 0;
static const char g_lua_ue4ss_shared_registry_key  = 0;

static bool lua_ue4ss_pending_lock             (lua_ue4ss_context_t      *context);
static void lua_ue4ss_pending_unlock           (lua_ue4ss_context_t      *context);
static void lua_ue4ss_pending_hook_destroy     (lua_ue4ss_pending_hook_t *pending);
static void lua_ue4ss_dispatch_pending_hooks   (lua_ue4ss_context_t      *context);
static void lua_ue4ss_dispatch_pending_inputs  (lua_ue4ss_context_t      *context);
static void lua_ue4ss_dispatch_pending_commands(lua_ue4ss_context_t      *context);
static void lua_ue4ss_dispatch_keybinds        (lua_ue4ss_context_t      *context, input_event_t *ev);
static void lua_ue4ss_dispatch_process_event   (lua_ue4ss_context_t      *context, uobject_t *obj, ufunc_t *func, void *params, bool post);

static const char *g_lua_ue4ss_key_constants[256] = {
  [0x01] = "LEFT_MOUSE_BUTTON",
  [0x02] = "RIGHT_MOUSE_BUTTON",
  [0x04] = "MIDDLE_MOUSE_BUTTON",
  [0x05] = "XBUTTON_ONE",
  [0x06] = "XBUTTON_TWO",
  [0x08] = "BACKSPACE",
  [0x09] = "TAB",
  [0x0D] = "RETURN",
  [0x13] = "PAUSE",
  [0x14] = "CAPS_LOCK",
  [0x1B] = "ESCAPE",
  [0x20] = "SPACE",
  [0x21] = "PAGE_UP",
  [0x22] = "PAGE_DOWN",
  [0x23] = "END",
  [0x24] = "HOME",
  [0x25] = "LEFT_ARROW",
  [0x26] = "UP_ARROW",
  [0x27] = "RIGHT_ARROW",
  [0x28] = "DOWN_ARROW",
  [0x2C] = "PRINT_SCREEN",
  [0x2D] = "INS",
  [0x2E] = "DEL",
  [0x30] = "ZERO",
  [0x31] = "ONE",
  [0x32] = "TWO",
  [0x33] = "THREE",
  [0x34] = "FOUR",
  [0x35] = "FIVE",
  [0x36] = "SIX",
  [0x37] = "SEVEN",
  [0x38] = "EIGHT",
  [0x39] = "NINE",
  [0x41] = "A",
  [0x42] = "B",
  [0x43] = "C",
  [0x44] = "D",
  [0x45] = "E",
  [0x46] = "F",
  [0x47] = "G",
  [0x48] = "H",
  [0x49] = "I",
  [0x4A] = "J",
  [0x4B] = "K",
  [0x4C] = "L",
  [0x4D] = "M",
  [0x4E] = "N",
  [0x4F] = "O",
  [0x50] = "P",
  [0x51] = "Q",
  [0x52] = "R",
  [0x53] = "S",
  [0x54] = "T",
  [0x55] = "U",
  [0x56] = "V",
  [0x57] = "W",
  [0x58] = "X",
  [0x59] = "Y",
  [0x5A] = "Z",
  [0x60] = "NUM_ZERO",
  [0x61] = "NUM_ONE",
  [0x62] = "NUM_TWO",
  [0x63] = "NUM_THREE",
  [0x64] = "NUM_FOUR",
  [0x65] = "NUM_FIVE",
  [0x66] = "NUM_SIX",
  [0x67] = "NUM_SEVEN",
  [0x68] = "NUM_EIGHT",
  [0x69] = "NUM_NINE",
  [0x6A] = "MULTIPLY",
  [0x6B] = "ADD",
  [0x6D] = "SUBTRACT",
  [0x6E] = "DECIMAL",
  [0x6F] = "DIVIDE",
  [0x70] = "F1",
  [0x71] = "F2",
  [0x72] = "F3",
  [0x73] = "F4",
  [0x74] = "F5",
  [0x75] = "F6",
  [0x76] = "F7",
  [0x77] = "F8",
  [0x78] = "F9",
  [0x79] = "F10",
  [0x7A] = "F11",
  [0x7B] = "F12",
  [0x90] = "NUM_LOCK",
  [0x91] = "SCROLL_LOCK",
  [0xBA] = "OEM_ONE",
  [0xBB] = "OEM_PLUS",
  [0xBC] = "OEM_COMMA",
  [0xBD] = "OEM_MINUS",
  [0xBE] = "OEM_PERIOD",
  [0xBF] = "OEM_TWO",
  [0xC0] = "OEM_THREE",
  [0xDB] = "OEM_FOUR",
  [0xDC] = "OEM_FIVE",
  [0xDD] = "OEM_SIX",
  [0xDE] = "OEM_SEVEN",
};

static lua_ue4ss_context_t *
lua_ue4ss_context_from_state(lua_State *state)
{
  lua_rawgetp(state, LUA_REGISTRYINDEX, &g_lua_ue4ss_context_registry_key);
  lua_ue4ss_context_t *context = (lua_ue4ss_context_t *)lua_touserdata(state, -1);
  lua_pop(state, 1);
  return context;
}

bool
lua_ue4ss_context_init(lua_ue4ss_context_t *context, arena_t *arena, lua_runtime_t *runtime)
{
  if (!context || !arena || !runtime) {
    return false;
  }

  context->arena                    = arena;
  context->runtime                  = runtime;
  context->commands                 = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_command_t, CONFIG_LUA_UE4SS_MAX_COMMANDS);
  context->command_count            = 0;
  context->keybinds                 = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_keybind_t, CONFIG_LUA_UE4SS_MAX_KEYBINDS);
  context->keybind_count            = 0;
  context->hooks                    = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_hook_t, CONFIG_LUA_UE4SS_MAX_HOOKS);
  context->hook_count               = 0;
  context->next_hook_id             = 1;
  context->notifications            = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_notification_t, CONFIG_LUA_UE4SS_MAX_NOTIFICATIONS);
  context->notification_count       = 0;
  context->pending_objects          = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_pending_object_t, CONFIG_LUA_UE4SS_MAX_PENDING_OBJECTS);
  context->pending_object_count     = 0;
  context->pending_hooks            = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_pending_hook_t, CONFIG_LUA_UE4SS_MAX_PENDING_HOOKS);
  context->pending_hook_count       = 0;
  context->pending_inputs           = ARENA_PUSH_ARRAY_ZERO(arena, input_event_t, CONFIG_LUA_UE4SS_MAX_PENDING_INPUTS);
  context->pending_input_count      = 0;
  context->pending_commands         = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_pending_command_t, CONFIG_LUA_UE4SS_MAX_PENDING_COMMANDS);
  context->pending_command_count    = 0;
  context->pending_lock             = 0;
  context->pending_overflow         = false;
  context->pending_hook_overflow    = false;
  context->pending_input_overflow   = false;
  context->pending_command_overflow = false;
  context->async_events             = false;
  context->scheduled_jobs           = ARENA_PUSH_ARRAY_ZERO(arena, lua_ue4ss_scheduled_job_t, CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS);
  context->scheduled_job_count      = 0;
  context->next_action_handle       = 1;
  context->scheduler_tick           = 0;
  context->async_scheduler_tick     = 0;
  context->wake_async               = NULL;
  context->wake_async_user          = NULL;

  return context->commands         &&
         context->keybinds         &&
         context->hooks            &&
         context->notifications    &&
         context->pending_objects  &&
         context->pending_hooks    &&
         context->pending_inputs   &&
         context->pending_commands &&
         context->scheduled_jobs;
}

void
lua_ue4ss_context_deinit(lua_ue4ss_context_t *context)
{
  if (!context) {
    return;
  }

  lua_State *state = context->runtime && context->runtime->inited ? context->runtime->state : NULL;
  if (state) {
    for (int i = 0; i < context->command_count; ++i) {
      int callback_ref = context->commands[i].callback_ref;
      if (callback_ref != LUA_NOREF && callback_ref != LUA_REFNIL) {
        luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
      }
    }

    for (int i = 0; i < context->keybind_count; ++i) {
      int callback_ref = context->keybinds[i].callback_ref;
      if (callback_ref != LUA_NOREF && callback_ref != LUA_REFNIL) {
        luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
      }
    }

    for (int i = 0; i < context->hook_count; ++i) {
      lua_ue4ss_hook_t *hook = &context->hooks[i];
      if (hook->pre_callback_ref != LUA_NOREF && hook->pre_callback_ref != LUA_REFNIL) {
        luaL_unref(state, LUA_REGISTRYINDEX, hook->pre_callback_ref);
      }

      if (hook->post_callback_ref != LUA_NOREF && hook->post_callback_ref != LUA_REFNIL) {
        luaL_unref(state, LUA_REGISTRYINDEX, hook->post_callback_ref);
      }
    }

    for (int i = 0; i < context->notification_count; ++i) {
      int callback_ref = context->notifications[i].callback_ref;
      if (callback_ref != LUA_NOREF && callback_ref != LUA_REFNIL) {
        luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
      }
    }

    if (context->scheduled_jobs) {
      for (int i = 0; i < CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS; ++i) {
        lua_ue4ss_scheduled_job_t *job = &context->scheduled_jobs[i];
        if (job->active && job->callback_ref != LUA_NOREF && job->callback_ref != LUA_REFNIL) {
          luaL_unref(state, LUA_REGISTRYINDEX, job->callback_ref);
        }
      }
    }
  }

  context->command_count        = 0;
  context->keybind_count        = 0;
  context->hook_count           = 0;
  context->notification_count   = 0;
  context->pending_object_count = 0;

  for (int i = 0; i < context->pending_hook_count; ++i) {
    lua_ue4ss_pending_hook_destroy(&context->pending_hooks[i]);
  }

  context->pending_hook_count  = 0;
  context->pending_input_count = 0;

  for (int i = 0; i < context->pending_command_count; ++i) {
    free(context->pending_commands[i].data);
    context->pending_commands[i] = (lua_ue4ss_pending_command_t){0};
  }

  context->pending_command_count = 0;
  context->scheduled_job_count   = 0;
}

static input_key_kind_t
lua_ue4ss_key_from_vk(lua_Integer value)
{
  if (value >= 0x41 && value <= 0x5A) {
    return (input_key_kind_t)(INPUT_KEY_A + value - 0x41);
  }

  if (value >= 0x31 && value <= 0x39) {
    return (input_key_kind_t)(INPUT_KEY_1 + value - 0x31);
  }

  if (value == 0x30) {
    return INPUT_KEY_0;
  }

  if (value >= 0x70 && value <= 0x7B) {
    return (input_key_kind_t)(INPUT_KEY_F1 + value - 0x70);
  }

  if (value >= 0x60 && value <= 0x69) {
    return (input_key_kind_t)(INPUT_KEY_NUM_0 + value - 0x60);
  }

  switch (value) {
    case 0x01: return INPUT_KEY_MOUSE_LEFT;
    case 0x02: return INPUT_KEY_MOUSE_RIGHT;
    case 0x04: return INPUT_KEY_MOUSE_MIDDLE;
    case 0x05: return INPUT_KEY_MOUSE_THUMB_1;
    case 0x06: return INPUT_KEY_MOUSE_THUMB_2;
    case 0x08: return INPUT_KEY_BACKSPACE;
    case 0x09: return INPUT_KEY_TAB;
    case 0x0D: return INPUT_KEY_ENTER;
    case 0x10: return INPUT_KEY_SHIFT;
    case 0x11: return INPUT_KEY_CTRL;
    case 0x12: return INPUT_KEY_ALT;
    case 0x13: return INPUT_KEY_PAUSE;
    case 0x14: return INPUT_KEY_CAPS_LOCK;
    case 0x1B: return INPUT_KEY_ESCAPE;
    case 0x20: return INPUT_KEY_SPACE;
    case 0x21: return INPUT_KEY_PAGE_UP;
    case 0x22: return INPUT_KEY_PAGE_DOWN;
    case 0x23: return INPUT_KEY_END;
    case 0x24: return INPUT_KEY_HOME;
    case 0x25: return INPUT_KEY_LEFT;
    case 0x26: return INPUT_KEY_UP;
    case 0x27: return INPUT_KEY_RIGHT;
    case 0x28: return INPUT_KEY_DOWN;
    case 0x2C: return INPUT_KEY_PRINT_SCREEN;
    case 0x2D: return INPUT_KEY_INSERT;
    case 0x2E: return INPUT_KEY_DELETE;
    case 0x6A: return INPUT_KEY_NUM_MULTIPLY;
    case 0x6B: return INPUT_KEY_NUM_ADD;
    case 0x6D: return INPUT_KEY_NUM_SUBTRACT;
    case 0x6E: return INPUT_KEY_NUM_DECIMAL;
    case 0x6F: return INPUT_KEY_NUM_DIVIDE;
    case 0x90: return INPUT_KEY_NUM_LOCK;
    case 0x91: return INPUT_KEY_SCROLL_LOCK;
    case 0xBA: return INPUT_KEY_SEMICOLON;
    case 0xBB: return INPUT_KEY_EQUALS;
    case 0xBC: return INPUT_KEY_COMMA;
    case 0xBD: return INPUT_KEY_HYPHEN;
    case 0xBE: return INPUT_KEY_PERIOD;
    case 0xBF: return INPUT_KEY_SLASH;
    case 0xC0: return INPUT_KEY_BACKTICK;
    case 0xDB: return INPUT_KEY_LEFT_BRACKET;
    case 0xDC: return INPUT_KEY_BACKSLASH;
    case 0xDD: return INPUT_KEY_RIGHT_BRACKET;
    case 0xDE: return INPUT_KEY_APOSTROPHE;
    default:   return INPUT_KEY_NONE;
  }
}

static bool
lua_ue4ss_bind_add_key(keybind_t *bind, input_key_kind_t key)
{
  if (!bind || key == INPUT_KEY_NONE || key >= INPUT_KEY_MAX || bind->count >= KEYBIND_MAX_KEYS) {
    return false;
  }

  for (int i = 0; i < bind->count; ++i) {
    if (bind->keys[i] == key) {
      return false;
    }
  }

  bind->keys[bind->count++] = key;
  return true;
}

static bool
lua_ue4ss_bind_add_value(lua_State *state, int idx, keybind_t *bind)
{
  idx = lua_absindex(state, idx);
  if (lua_isinteger(state, idx)) {
    return lua_ue4ss_bind_add_key(bind, lua_ue4ss_key_from_vk(lua_tointeger(state, idx)));
  }

  if (lua_type(state, idx) == LUA_TSTRING) {
    size_t      len    = 0;
    const char *data   = lua_tolstring(state, idx, &len);
    keybind_t   parsed = keybind_parse(str_make((void *)data, (uint64_t)len), KEYBIND_NULL);
    if (!keybind_is_valid(parsed)) {
      return false;
    }

    for (int i = 0; i < parsed.count; ++i) {
      if (!lua_ue4ss_bind_add_key(bind, parsed.keys[i])) {
        return false;
      }
    }
    return true;
  }
  return false;
}

static bool
lua_ue4ss_bind_from_args(lua_State *state, int key_idx, int modifiers_idx, keybind_t *out)
{
  keybind_t bind = KEYBIND_NULL;
  if (!lua_ue4ss_bind_add_value(state, key_idx, &bind)) {
    return false;
  }

  if (modifiers_idx > 0 && !lua_isnoneornil(state, modifiers_idx)) {
    if (!lua_istable(state, modifiers_idx)) {
      return false;
    }

    lua_Integer count = (lua_Integer)lua_rawlen(state, modifiers_idx);
    for (lua_Integer i = 1; i <= count; ++i) {
      lua_rawgeti(state, modifiers_idx, i);
      bool added = lua_ue4ss_bind_add_value(state, -1, &bind);
      lua_pop(state, 1);
      if (!added) {
        return false;
      }
    }
  }

  if (!keybind_is_valid(bind)) {
    return false;
  }

  *out = bind;
  return true;
}

static int
lua_ue4ss_register_console_command(lua_State *state)
{
  lua_ue4ss_context_t *context   = lua_ue4ss_context_from_state(state);
  size_t               name_len  = 0;
  const char          *name_data = luaL_checklstring(state, 1, &name_len);

  luaL_checktype(state, 2, LUA_TFUNCTION);
  if (!context || !context->arena || !context->commands) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  if (name_len == 0) {
    return luaL_error(state, "console command name cannot be empty");
  }

  str_t name       = str_make((void *)name_data, (uint64_t)name_len);
  str_t saved_name = str_push_copy(context->arena, name);
  if (str_is_empty(saved_name)) {
    return luaL_error(state, "failed to allocate console command name");
  }

  if (!lua_ue4ss_pending_lock(context)) {
    return luaL_error(state, "failed to lock Lua console commands");
  }

  for (int i = 0; i < context->command_count; ++i) {
    if (str_equal(context->commands[i].name, name, STR_CMP_FLAG_IGNORE_CASE)) {
      lua_ue4ss_pending_unlock(context);
      return luaL_error(state, "console command '%.*s' is already registered by this mod", (int)name.len, name.data);
    }
  }

  if (context->command_count >= CONFIG_LUA_UE4SS_MAX_COMMANDS) {
    lua_ue4ss_pending_unlock(context);
    return luaL_error(state, "Lua console command limit reached");
  }

  lua_ue4ss_command_t *command = &context->commands[context->command_count++];

  command->name = saved_name;
  lua_pushvalue(state, 2);

  command->callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_ue4ss_pending_unlock(context);
  return 0;
}

static int64_t
lua_ue4ss_next_hook_id(lua_ue4ss_context_t *context)
{
  int64_t result = context->next_hook_id++;
  if (context->next_hook_id <= 0 || context->next_hook_id > (int64_t)LUA_MAXINTEGER) {
    context->next_hook_id = 1;
  }
  return result;
}

static ufunc_t *
lua_ue4ss_find_hook_function(str_t target)
{
  ufunc_t *function = (ufunc_t *)unreal_uobject_find_by_full_name(globals.unreal.core_func, target);
  if (function) {
    return function;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t lookup = str_push_copy(tmp.arena, target);
    for (uint64_t i = lookup.len; i > 0; --i) {
      if (lookup.data[i - 1] == ':') {
        lookup.data[i - 1] = '.';
        break;
      }
    }

    function = (ufunc_t *)unreal_uobject_find_by_full_name(globals.unreal.core_func, lookup);
  }
  scratch_end(tmp);

  return function;
}

static int
lua_ue4ss_register_hook(lua_State *state)
{
  lua_ue4ss_context_t *context     = lua_ue4ss_context_from_state(state);
  size_t               target_len  = 0;
  const char          *target_data = luaL_checklstring(state, 1, &target_len);
  bool                 has_pre     = lua_type(state, 2) == LUA_TFUNCTION;
  bool                 has_post    = lua_type(state, 3) == LUA_TFUNCTION;

  if (!context || !context->arena || !context->hooks) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  if (target_len == 0) {
    return luaL_error(state, "UFunction name cannot be empty");
  }

  if (!lua_isnoneornil(state, 2) && !has_pre) {
    return luaL_argerror(state, 2, "expected a function or nil");
  }

  if (!lua_isnoneornil(state, 3) && !has_post) {
    return luaL_argerror(state, 3, "expected a function or nil");
  }

  if (!has_pre && !has_post) {
    return luaL_error(state, "RegisterHook requires a pre or post callback");
  }

  if (context->hook_count >= CONFIG_LUA_UE4SS_MAX_HOOKS) {
    return luaL_error(state, "Lua UFunction hook limit reached");
  }

  str_t    target   = str_make((void *)target_data, (uint64_t)target_len);
  ufunc_t *function = lua_ue4ss_find_hook_function(target);
  if (!function) {
    return luaL_error(state, "UFunction '%.*s' was not found", (int)target.len, target.data);
  }

  fweak_object_ptr_t function_weak = {.object_idx = -1};
  if (!unreal_fweak_object_from_object((uobject_t *)function, &function_weak)) {
    return luaL_error(state, "UFunction '%.*s' is not a valid UObject", (int)target.len, target.data);
  }

  str_t saved_target = str_push_copy(context->arena, target);
  if (str_is_empty(saved_target)) {
    return luaL_error(state, "failed to allocate UFunction hook name");
  }

  if (!lua_ue4ss_pending_lock(context)) {
    return luaL_error(state, "failed to lock Lua UFunction hooks");
  }

  if (context->hook_count >= CONFIG_LUA_UE4SS_MAX_HOOKS) {
    lua_ue4ss_pending_unlock(context);
    return luaL_error(state, "Lua UFunction hook limit reached");
  }

  lua_ue4ss_hook_t *hook = &context->hooks[context->hook_count++];
  *hook = (lua_ue4ss_hook_t){
    .target            = saved_target,
    .function          = function_weak,
    .pre_id            = lua_ue4ss_next_hook_id(context),
    .post_id           = lua_ue4ss_next_hook_id(context),
    .pre_callback_ref  = LUA_NOREF,
    .post_callback_ref = LUA_NOREF,
    .active            = true,
  };

  if (has_pre) {
    lua_pushvalue(state, 2);
    hook->pre_callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  }

  if (has_post) {
    lua_pushvalue(state, 3);
    hook->post_callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  }

  lua_ue4ss_pending_unlock(context);

  lua_pushinteger(state, (lua_Integer)hook->pre_id);
  lua_pushinteger(state, (lua_Integer)hook->post_id);
  return 2;
}

static int
lua_ue4ss_unregister_hook(lua_State *state)
{
  lua_ue4ss_context_t *context     = lua_ue4ss_context_from_state(state);
  size_t               target_len  = 0;
  const char          *target_data = luaL_checklstring(state, 1, &target_len);
  lua_Integer          pre_id      = luaL_checkinteger(state, 2);
  lua_Integer          post_id     = luaL_checkinteger(state, 3);

  if (!context || !context->hooks) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  str_t target = str_make((void *)target_data, (uint64_t)target_len);
  if (!lua_ue4ss_pending_lock(context)) {
    return luaL_error(state, "failed to lock Lua UFunction hooks");
  }

  for (int i = 0; i < context->hook_count; ++i) {
    lua_ue4ss_hook_t *hook = &context->hooks[i];
    if (!hook->active || hook->pre_id != (int64_t)pre_id || hook->post_id != (int64_t)post_id || !str_equal(hook->target, target, 0)) {
      continue;
    }

    if (hook->pre_callback_ref != LUA_NOREF && hook->pre_callback_ref != LUA_REFNIL) {
      luaL_unref(state, LUA_REGISTRYINDEX, hook->pre_callback_ref);
    }

    if (hook->post_callback_ref != LUA_NOREF && hook->post_callback_ref != LUA_REFNIL) {
      luaL_unref(state, LUA_REGISTRYINDEX, hook->post_callback_ref);
    }

    hook->pre_callback_ref  = LUA_NOREF;
    hook->post_callback_ref = LUA_NOREF;
    hook->active            = false;
    lua_ue4ss_pending_unlock(context);
    return 0;
  }

  lua_ue4ss_pending_unlock(context);
  return 0;
}

static int
lua_ue4ss_notify_on_new_object(lua_State *state)
{
  lua_ue4ss_context_t *context         = lua_ue4ss_context_from_state(state);
  size_t               class_path_len  = 0;
  const char          *class_path_data = luaL_checklstring(state, 1, &class_path_len);

  luaL_checktype(state, 2, LUA_TFUNCTION);
  if (!context || !context->arena || !context->notifications) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  if (class_path_len == 0) {
    return luaL_error(state, "UClass name cannot be empty");
  }

  str_t     class_path = str_make((void *)class_path_data, (uint64_t)class_path_len);
  uclass_t *target     = unreal_uobject_find_class_by_full_name(class_path);
  if (!target) {
    return luaL_error(state, "UClass '%.*s' was not found", (int)class_path.len, class_path.data);
  }

  fweak_object_ptr_t target_weak = {.object_idx = -1};
  if (!unreal_fweak_object_from_object((uobject_t *)target, &target_weak)) {
    return luaL_error(state, "UClass '%.*s' is not a valid UObject", (int)class_path.len, class_path.data);
  }

  str_t saved_path = str_push_copy(context->arena, class_path);
  if (str_is_empty(saved_path)) {
    return luaL_error(state, "failed to allocate UClass notification name");
  }

  lua_pushvalue(state, 2);
  int callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  if (!lua_ue4ss_pending_lock(context)) {
    luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
    return luaL_error(state, "failed to lock Lua new-object notifications");
  }

  if (context->notification_count >= CONFIG_LUA_UE4SS_MAX_NOTIFICATIONS) {
    lua_ue4ss_pending_unlock(context);
    luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
    return luaL_error(state, "Lua new-object notification limit reached");
  }

  lua_ue4ss_notification_t *notification = &context->notifications[context->notification_count++];
  *notification = (lua_ue4ss_notification_t){
    .class_path   = saved_path,
    .target_class = target_weak,
    .callback_ref = callback_ref,
    .active       = true,
  };
  lua_ue4ss_pending_unlock(context);

  return 0;
}

static lua_ue4ss_scheduled_job_t *
lua_ue4ss_find_scheduled_job(lua_ue4ss_context_t *context, int64_t handle)
{
  if (!context || !context->scheduled_jobs || handle <= 0) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS; ++i) {
    lua_ue4ss_scheduled_job_t *job = &context->scheduled_jobs[i];
    if (job->active && job->handle == handle) {
      return job;
    }
  }
  return NULL;
}

static void
lua_ue4ss_disable_scheduled_job(lua_ue4ss_context_t *context, lua_ue4ss_scheduled_job_t *job);

static int64_t
lua_ue4ss_next_action_handle(lua_ue4ss_context_t *context)
{
  for (int attempt = 0; attempt <= CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS; ++attempt) {
    int64_t handle = context->next_action_handle++;
    if (context->next_action_handle <= 0 || context->next_action_handle > (int64_t)LUA_MAXINTEGER) {
      context->next_action_handle = 1;
    }

    if (!lua_ue4ss_find_scheduled_job(context, handle)) {
      return handle;
    }
  }
  return 0;
}

static uint64_t
lua_ue4ss_check_delay_ms(lua_State *state, int idx)
{
  lua_Integer delay_ms = luaL_checkinteger(state, idx);
  if (delay_ms < 0) {
    luaL_argerror(state, idx, "delay cannot be negative");
  }

  if ((uint64_t)delay_ms > UINT64_MAX / 1000ULL) {
    luaL_argerror(state, idx, "delay is too large");
  }
  return (uint64_t)delay_ms;
}

static int
lua_ue4ss_schedule_job(lua_State *state, uint64_t delay_ms, int callback_idx, bool loop, bool game_thread, int64_t requested_handle, bool return_handle)
{
  lua_ue4ss_context_t *context = lua_ue4ss_context_from_state(state);
  luaL_checktype(state, callback_idx, LUA_TFUNCTION);
  if (!context || !context->scheduled_jobs) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  if (requested_handle > 0 && lua_ue4ss_find_scheduled_job(context, requested_handle)) {
    if (return_handle) {
      lua_pushinteger(state, (lua_Integer)requested_handle);
      return 1;
    }
    return 0;
  }

  lua_ue4ss_scheduled_job_t *job = NULL;
  for (int i = 0; i < CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS; ++i) {
    if (!context->scheduled_jobs[i].active) {
      job = &context->scheduled_jobs[i];
      break;
    }
  }

  if (!job) {
    return luaL_error(state, "Lua scheduled-job limit reached");
  }

  int64_t handle = requested_handle > 0 ? requested_handle : lua_ue4ss_next_action_handle(context);
  if (handle <= 0) {
    return luaL_error(state, "failed to allocate a scheduled-job handle");
  }

  uint64_t now          = time_now_us();
  uint64_t delay_us     = delay_ms * 1000ULL;
  uint64_t due_us       = delay_us > UINT64_MAX - now ? UINT64_MAX : now + delay_us;
  uint64_t current_tick = game_thread ? context->scheduler_tick : context->async_scheduler_tick;
  uint64_t ready_tick   = current_tick == UINT64_MAX ? UINT64_MAX : current_tick + 1;

  lua_pushvalue(state, callback_idx);
  int callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  *job = (lua_ue4ss_scheduled_job_t){
    .due_us       = due_us,
    .interval_us  = delay_us,
    .ready_tick   = ready_tick,
    .handle       = handle,
    .callback_ref = callback_ref,
    .loop         = loop,
    .game_thread  = game_thread,
    .active       = true,
  };
  context->scheduled_job_count += 1;

  if (!game_thread && context->wake_async) {
    context->wake_async(context->wake_async_user);
  }

  if (return_handle) {
    lua_pushinteger(state, (lua_Integer)handle);
    return 1;
  }
  return 0;
}

static int
lua_ue4ss_execute_in_game_thread(lua_State *state)
{
  return lua_ue4ss_schedule_job(state, 0, 1, false, true, 0, false);
}

static int
lua_ue4ss_is_in_game_thread(lua_State *state)
{
  bool in_game_thread = unreal_is_in_game_thread();
  lua_pushboolean(state, in_game_thread);
  return 1;
}

static int
lua_ue4ss_execute_in_game_thread_with_delay(lua_State *state)
{
  int arg_count = lua_gettop(state);
  if (arg_count == 2) {
    uint64_t delay_ms = lua_ue4ss_check_delay_ms(state, 1);
    return lua_ue4ss_schedule_job(state, delay_ms, 2, false, true, 0, true);
  }

  if (arg_count == 3) {
    lua_Integer handle = luaL_checkinteger(state, 1);
    if (handle <= 0) {
      return luaL_argerror(state, 1, "handle must be positive");
    }

    uint64_t delay_ms = lua_ue4ss_check_delay_ms(state, 2);
    return lua_ue4ss_schedule_job(state, delay_ms, 3, false, true, (int64_t)handle, true);
  }
  return luaL_error(state, "expected (delay_ms, callback) or (handle, delay_ms, callback)");
}

static int
lua_ue4ss_execute_async(lua_State *state)
{
  return lua_ue4ss_schedule_job(state, 0, 1, false, false, 0, false);
}

static int
lua_ue4ss_execute_with_delay(lua_State *state)
{
  uint64_t delay_ms = lua_ue4ss_check_delay_ms(state, 1);
  return lua_ue4ss_schedule_job(state, delay_ms, 2, false, false, 0, false);
}

static int
lua_ue4ss_loop_async(lua_State *state)
{
  uint64_t delay_ms = lua_ue4ss_check_delay_ms(state, 1);
  return lua_ue4ss_schedule_job(state, delay_ms, 2, true, false, 0, false);
}

static int
lua_ue4ss_loop_in_game_thread_after_frames(lua_State *state)
{
  lua_Integer frames = luaL_checkinteger(state, 1);
  if (frames < 0) {
    return luaL_argerror(state, 1, "frame count cannot be negative");
  }

  uint64_t interval_ticks = MAX_VAL((uint64_t)frames, 1ULL);
  int      result         = lua_ue4ss_schedule_job(state, 0, 2, true, true, 0, true);
  if (result == 1) {
    lua_ue4ss_context_t       *context = lua_ue4ss_context_from_state(state);
    int64_t                    handle  = (int64_t)lua_tointeger(state, -1);
    lua_ue4ss_scheduled_job_t *job     = lua_ue4ss_find_scheduled_job(context, handle);
    if (job) {
      job->frame_based    = true;
      job->interval_ticks = interval_ticks;
      job->ready_tick     = interval_ticks > UINT64_MAX - context->scheduler_tick ? UINT64_MAX : context->scheduler_tick + interval_ticks;
    }
  }
  return result;
}

static int
lua_ue4ss_cancel_delayed_action(lua_State *state)
{
  lua_ue4ss_context_t *context = lua_ue4ss_context_from_state(state);
  lua_Integer          handle  = luaL_checkinteger(state, 1);
  if (handle <= 0) {
    return luaL_argerror(state, 1, "handle must be positive");
  }

  lua_ue4ss_scheduled_job_t *job       = lua_ue4ss_find_scheduled_job(context, (int64_t)handle);
  bool                       cancelled = job != NULL;
  if (job) {
    lua_ue4ss_disable_scheduled_job(context, job);
  }
  lua_pushboolean(state, cancelled);
  return 1;
}

static int
lua_ue4ss_register_keybind(lua_State *state)
{
  lua_ue4ss_context_t *context       = lua_ue4ss_context_from_state(state);
  int                  callback_idx  = lua_istable(state, 2) ? 3 : 2;
  int                  modifiers_idx = callback_idx == 3 ? 2 : 0;
  luaL_checktype(state, callback_idx, LUA_TFUNCTION);
  if (!context || !context->keybinds) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  keybind_t bind = KEYBIND_NULL;
  if (!lua_ue4ss_bind_from_args(state, 1, modifiers_idx, &bind)) {
    return luaL_error(state, "invalid keybind; use a UE4SS Key value or an Overdub key string");
  }

  for (int i = 0; i < bind.count; ++i) {
    if (!input_key_is_bindable(bind.keys[i])) {
      return luaL_error(state, "keybind contains a non-bindable input axis");
    }
  }

  if (!lua_ue4ss_pending_lock(context)) {
    return luaL_error(state, "failed to lock Lua keybinds");
  }

  for (int i = 0; i < context->keybind_count; ++i) {
    if (keybind_equal(context->keybinds[i].bind, bind)) {
      lua_ue4ss_pending_unlock(context);
      lua_pushboolean(state, false);
      return 1;
    }
  }

  if (context->keybind_count >= CONFIG_LUA_UE4SS_MAX_KEYBINDS) {
    lua_ue4ss_pending_unlock(context);
    return luaL_error(state, "Lua keybind limit reached");
  }

  lua_ue4ss_keybind_t *keybind = &context->keybinds[context->keybind_count++];

  keybind->bind = bind;
  lua_pushvalue(state, callback_idx);

  keybind->callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_ue4ss_pending_unlock(context);
  lua_pushboolean(state, true);
  return 1;
}

static int
lua_ue4ss_is_keybind_registered(lua_State *state)
{
  lua_ue4ss_context_t *context       = lua_ue4ss_context_from_state(state);
  int                  modifiers_idx = lua_istable(state, 2) ? 2 : 0;
  keybind_t            bind          = KEYBIND_NULL;
  if (!context || !lua_ue4ss_bind_from_args(state, 1, modifiers_idx, &bind)) {
    lua_pushboolean(state, false);
    return 1;
  }

  if (!lua_ue4ss_pending_lock(context)) {
    lua_pushboolean(state, false);
    return 1;
  }

  for (int i = 0; i < context->keybind_count; ++i) {
    if (keybind_equal(context->keybinds[i].bind, bind)) {
      lua_ue4ss_pending_unlock(context);
      lua_pushboolean(state, true);
      return 1;
    }
  }

  lua_ue4ss_pending_unlock(context);
  lua_pushboolean(state, false);
  return 1;
}

static void
lua_ue4ss_set_integer_field(lua_State *state, const char *name, lua_Integer value)
{
  lua_pushinteger(state, value);
  lua_setfield(state, -2, name);
}

static void
lua_ue4ss_push_command_parts(lua_State *state, str_t args)
{
  lua_newtable(state);
  int table_idx = lua_absindex(state, -1);
  int part_idx  = 1;
  uint64_t pos  = 0;

  while (pos < args.len) {
    while (pos < args.len && (args.data[pos] == ' ' || args.data[pos] == '\t' || args.data[pos] == '\r' || args.data[pos] == '\n')) {
      pos += 1;
    }

    if (pos >= args.len) {
      break;
    }

    uint8_t quote = 0;
    if (args.data[pos] == '\'' || args.data[pos] == '"') {
      quote = args.data[pos++];
    }

    luaL_Buffer buffer;
    luaL_buffinit(state, &buffer);
    while (pos < args.len) {
      uint8_t ch = args.data[pos];
      if (quote) {
        if (ch == quote) {
          pos += 1;
          break;
        }
      } else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
        break;
      }

      if (ch == '\\' && pos + 1 < args.len && (args.data[pos + 1] == quote || args.data[pos + 1] == '\\')) {
        pos += 1;
        ch = args.data[pos];
      }

      luaL_addchar(&buffer, (char)ch);
      pos += 1;
    }

    luaL_pushresult(&buffer);
    lua_rawseti(state, table_idx, part_idx++);
  }
}

static void
lua_ue4ss_set_absolute_path(lua_State *state, int table_idx, str_t path)
{
  table_idx = lua_absindex(state, table_idx);
  lua_pushlstring(state, (const char *)path.data, (size_t)path.len);
  lua_setfield(state, table_idx, "__absolute_path");
}

static void
lua_ue4ss_push_directory(lua_State *state, str_t path, int depth, int *node_budget)
{
  lua_newtable(state);
  lua_ue4ss_set_absolute_path(state, -1, path);

  if (depth <= 0 || !node_budget || *node_budget <= 0 || !dir_exists(path)) {
    return;
  }

  *node_budget -= 1;

  int         table_idx = lua_absindex(state, -1);
  tmp_arena_t tmp       = scratch_begin(NULL);
  {
    dir_entry_list_t entries = dir_list(path, tmp.arena);
    for (int i = 0; i < entries.count && *node_budget > 0; ++i) {
      dir_entry_t *entry = &entries.items[i];
      if (entry->kind != DIR_ENTRY_DIR) {
        continue;
      }

      lua_pushlstring(state, (const char *)entry->name.data, (size_t)entry->name.len);
      lua_ue4ss_push_directory(state, entry->path, depth - 1, node_budget);
      lua_settable(state, table_idx);
    }
  }
  scratch_end(tmp);
}

static int
lua_ue4ss_iterate_game_directories(lua_State *state)
{
  lua_ue4ss_context_t *context = lua_ue4ss_context_from_state(state);
  if (!context) {
    return luaL_error(state, "Lua mod context is unavailable");
  }

  lua_newtable(state); // root
  int root_idx = lua_absindex(state, -1);

  lua_newtable(state); // Game
  int game_idx = lua_absindex(state, -1);

  lua_newtable(state); // Binaries
  int binaries_idx = lua_absindex(state, -1);

  lua_newtable(state); // Win64
  int win64_idx = lua_absindex(state, -1);
  lua_ue4ss_set_absolute_path(state, win64_idx, context->game_dir);

  lua_newtable(state); // virtual Mods
  int mods_idx = lua_absindex(state, -1);
  lua_ue4ss_set_absolute_path(state, mods_idx, context->root_mod_dir);

  int node_budget = LUA_UE4SS_DIRECTORY_MAX_NODES;
  lua_ue4ss_push_directory(state, context->mod_dir, LUA_UE4SS_DIRECTORY_MAX_DEPTH, &node_budget);
  int mod_directory_idx = lua_absindex(state, -1);

  lua_pushvalue(state, mod_directory_idx);
  lua_pushlstring(state, (const char *)context->mod_dir_name.data, (size_t)context->mod_dir_name.len);
  lua_insert(state, -2);
  lua_settable(state, mods_idx);

  if (!str_equal(context->mod_id, context->mod_dir_name, 0)) {
    lua_pushlstring(state, (const char *)context->mod_id.data, (size_t)context->mod_id.len);
    lua_pushvalue(state, mod_directory_idx);
    lua_settable(state, mods_idx);
  }
  lua_pop(state, 1); // original mod directory table

  lua_setfield(state, win64_idx, "Mods");
  lua_setfield(state, binaries_idx, "Win64");
  lua_setfield(state, game_idx, "Binaries");
  lua_setfield(state, root_idx, "Game");
  return 1;
}

static int
lua_ue4ss_shared_get(lua_State *state)
{
  luaL_checkany(state, 2);
  lua_rawgetp(state, LUA_REGISTRYINDEX, &g_lua_ue4ss_shared_registry_key);
  lua_pushvalue(state, 2);
  lua_rawget(state, -2);
  lua_remove(state, -2);
  return 1;
}

static int
lua_ue4ss_shared_set(lua_State *state)
{
  luaL_checkany(state, 2);
  luaL_checkany(state, 3);
  lua_rawgetp(state, LUA_REGISTRYINDEX, &g_lua_ue4ss_shared_registry_key);
  lua_pushvalue(state, 2);
  lua_pushvalue(state, 3);
  lua_rawset(state, -3);
  return 0;
}

static void
lua_ue4ss_buffer_add_pattern(luaL_Buffer *buffer, str_t directory, const char *suffix)
{
  if (str_is_empty(directory)) {
    return;
  }

  if (buffer->n > 0) {
    luaL_addchar(buffer, ';');
  }
  luaL_addlstring(buffer, (const char *)directory.data, (size_t)directory.len);
  luaL_addchar(buffer, '/');
  luaL_addstring(buffer, suffix);
}

static void
lua_ue4ss_set_package_paths(lua_State *state, lua_ue4ss_context_t *context)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t entry_dir    = path_dir(context->entry_path);
    str_t shared       = path_join(tmp.arena, context->root_mod_dir, STR_LIT("shared"));
    str_t local_shared = path_join(tmp.arena, context->mod_dir, STR_LIT("shared"));
    str_t scripts      = path_join(tmp.arena, context->mod_dir, STR_LIT("scripts"));

    lua_getglobal(state, "package");
    int package_idx = lua_absindex(state, -1);

    luaL_Buffer path_buffer;
    luaL_buffinit(state, &path_buffer);
    lua_ue4ss_buffer_add_pattern(&path_buffer, entry_dir, "?.lua");
    lua_ue4ss_buffer_add_pattern(&path_buffer, entry_dir, "?/init.lua");

    if (!path_equal(scripts, entry_dir)) {
      lua_ue4ss_buffer_add_pattern(&path_buffer, scripts, "?.lua");
      lua_ue4ss_buffer_add_pattern(&path_buffer, scripts, "?/init.lua");
    }

    lua_ue4ss_buffer_add_pattern(&path_buffer, shared, "?.lua");
    lua_ue4ss_buffer_add_pattern(&path_buffer, shared, "?/?.lua");
    lua_ue4ss_buffer_add_pattern(&path_buffer, shared, "?/init.lua");
    lua_ue4ss_buffer_add_pattern(&path_buffer, local_shared, "?.lua");
    lua_ue4ss_buffer_add_pattern(&path_buffer, local_shared, "?/?.lua");
    luaL_pushresult(&path_buffer);
    lua_setfield(state, package_idx, "path");

    luaL_Buffer cpath_buffer;
    luaL_buffinit(state, &cpath_buffer);
    lua_ue4ss_buffer_add_pattern(&cpath_buffer, context->game_dir, "?.dll");
    lua_ue4ss_buffer_add_pattern(&cpath_buffer, context->mod_dir, "?.dll");
    lua_ue4ss_buffer_add_pattern(&cpath_buffer, entry_dir, "?.dll");
    lua_ue4ss_buffer_add_pattern(&cpath_buffer, context->root_mod_dir, "?.dll");
    luaL_pushresult(&cpath_buffer);
    lua_setfield(state, package_idx, "cpath");
    lua_pop(state, 1);
  }
  scratch_end(tmp);
}

bool
lua_ue4ss_register(lua_State *state, lua_ue4ss_context_t *context)
{
  if (!state                     ||
      !context                   ||
      !context->runtime          ||
      !context->arena            ||
      !context->commands         ||
      !context->keybinds         ||
      !context->hooks            ||
      !context->notifications    ||
      !context->pending_objects  ||
      !context->pending_hooks    ||
      !context->pending_inputs   ||
      !context->pending_commands ||
      !context->scheduled_jobs) {
    return false;
  }

  lua_pushlightuserdata(state, context);
  lua_rawsetp(state, LUA_REGISTRYINDEX, &g_lua_ue4ss_context_registry_key);

  lua_newtable(state);
  lua_rawsetp(state, LUA_REGISTRYINDEX, &g_lua_ue4ss_shared_registry_key);

  lua_newtable(state);
  lua_pushcfunction(state, lua_ue4ss_shared_get);
  lua_setfield(state, -2, "GetSharedVariable");
  lua_pushcfunction(state, lua_ue4ss_shared_set);
  lua_setfield(state, -2, "SetSharedVariable");
  lua_setglobal(state, "ModRef");

  lua_pushcfunction(state, lua_ue4ss_iterate_game_directories);
  lua_setglobal(state, "IterateGameDirectories");

  lua_pushcfunction(state, lua_ue4ss_register_console_command);
  lua_setglobal(state, "RegisterConsoleCommandHandler");
  lua_pushcfunction(state, lua_ue4ss_register_console_command);
  lua_setglobal(state, "RegisterConsoleCommandGlobalHandler");
  lua_pushcfunction(state, lua_ue4ss_register_keybind);
  lua_setglobal(state, "RegisterKeyBind");
  lua_pushcfunction(state, lua_ue4ss_register_keybind);
  lua_setglobal(state, "RegisterKeyBindAsync");
  lua_pushcfunction(state, lua_ue4ss_is_keybind_registered);
  lua_setglobal(state, "IsKeyBindRegistered");
  lua_pushcfunction(state, lua_ue4ss_register_hook);
  lua_setglobal(state, "RegisterHook");
  lua_pushcfunction(state, lua_ue4ss_unregister_hook);
  lua_setglobal(state, "UnregisterHook");
  lua_pushcfunction(state, lua_ue4ss_notify_on_new_object);
  lua_setglobal(state, "NotifyOnNewObject");
  lua_pushcfunction(state, lua_ue4ss_execute_in_game_thread);
  lua_setglobal(state, "ExecuteInGameThread");
  lua_pushcfunction(state, lua_ue4ss_is_in_game_thread);
  lua_setglobal(state, "IsInGameThread");
  lua_pushcfunction(state, lua_ue4ss_execute_in_game_thread_with_delay);
  lua_setglobal(state, "ExecuteInGameThreadWithDelay");
  lua_pushcfunction(state, lua_ue4ss_execute_async);
  lua_setglobal(state, "ExecuteAsync");
  lua_pushcfunction(state, lua_ue4ss_execute_with_delay);
  lua_setglobal(state, "ExecuteWithDelay");
  lua_pushcfunction(state, lua_ue4ss_loop_async);
  lua_setglobal(state, "LoopAsync");
  lua_pushcfunction(state, lua_ue4ss_loop_in_game_thread_after_frames);
  lua_setglobal(state, "LoopInGameThreadAfterFrames");
  lua_pushcfunction(state, lua_ue4ss_cancel_delayed_action);
  lua_setglobal(state, "CancelDelayedAction");

  lua_newtable(state);
  for (int i = 0; i < COUNTOF(g_lua_ue4ss_key_constants); ++i) {
    const char *name = g_lua_ue4ss_key_constants[i];
    if (name) {
      lua_pushinteger(state, i);
      lua_setfield(state, -2, name);
    }
  }
  lua_setglobal(state, "Key");

  lua_newtable(state);
  lua_ue4ss_set_integer_field(state, "SHIFT", 0x10);
  lua_ue4ss_set_integer_field(state, "CONTROL", 0x11);
  lua_ue4ss_set_integer_field(state, "ALT", 0x12);
  lua_setglobal(state, "ModifierKey");

  lua_ue4ss_set_package_paths(state, context);
  return true;
}

static void
lua_ue4ss_disable_callback(lua_ue4ss_context_t *context, int *callback_ref)
{
  if (*callback_ref != LUA_NOREF && *callback_ref != LUA_REFNIL) {
    luaL_unref(context->runtime->state, LUA_REGISTRYINDEX, *callback_ref);
  }
  *callback_ref = LUA_NOREF;
}

static bool
lua_ue4ss_call_ref(lua_ue4ss_context_t *context, int callback_ref, int arg_count, int result_count, uint64_t execution_limit_us,
                   bool log_error, uint64_t suppressed_error_hash, uint64_t *out_error_hash)
{
  uint64_t previous_limit = context->runtime->execution_limit_us;
  context->runtime->execution_limit_us = execution_limit_us;

  bool called = lua_runtime_call_ref_ex(context->runtime, callback_ref, arg_count, result_count, log_error, suppressed_error_hash, out_error_hash);
  context->runtime->execution_limit_us = previous_limit;
  return called;
}

static void
lua_ue4ss_disable_scheduled_job(lua_ue4ss_context_t *context, lua_ue4ss_scheduled_job_t *job)
{
  if (!context || !job || !job->active) {
    return;
  }

  if (job->callback_ref != LUA_NOREF && job->callback_ref != LUA_REFNIL) {
    luaL_unref(context->runtime->state, LUA_REGISTRYINDEX, job->callback_ref);
  }

  *job = (lua_ue4ss_scheduled_job_t){
    .callback_ref = LUA_NOREF,
  };
  context->scheduled_job_count = MAX_VAL(context->scheduled_job_count - 1, 0);
}

static void
lua_ue4ss_dispatch_scheduled_jobs(lua_ue4ss_context_t *context, bool game_thread)
{
  uint64_t *scheduler_tick = game_thread ? &context->scheduler_tick : &context->async_scheduler_tick;
  if (*scheduler_tick < UINT64_MAX) {
    *scheduler_tick += 1;
  }

  if (context->scheduled_job_count <= 0) {
    return;
  }

  lua_State *state          = context->runtime->state;
  uint64_t   dispatch_start = time_now_us();
  uint64_t   now            = dispatch_start;
  for (int i = 0; i < CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS; ++i) {
    lua_ue4ss_scheduled_job_t *job = &context->scheduled_jobs[i];
    if (!job->active || job->game_thread != game_thread || job->ready_tick > *scheduler_tick || job->due_us > now) {
      continue;
    }

    int      stack_base          = lua_gettop(state);
    int64_t  job_handle          = job->handle;
    bool     job_loop            = job->loop;
    bool     slow_warned         = job->slow_warned;
    uint32_t previous_errors     = job->consecutive_errors;
    uint64_t callback_error_hash = 0;
    uint64_t callback_start      = time_now_us();
    uint64_t execution_limit     = CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US;

    bool called = lua_ue4ss_call_ref(context, job->callback_ref, 0, job_loop ? 1 : 0, execution_limit, true, job->last_error_hash, &callback_error_hash);

    uint64_t callback_end = time_now_us();
    uint64_t callback_us  = callback_end >= callback_start ? callback_end - callback_start : 0;
    /* UE4SS's legacy async loop stops on true. Its handle-based game-thread loops ignore return values and remain active after callback errors */
    bool     stop_loop    = !game_thread && job_loop && called && lua_toboolean(state, -1);
    lua_settop(state, stack_base);

    if (game_thread && callback_us > CONFIG_LUA_UE4SS_GAME_DISPATCH_BUDGET_US && !slow_warned) {
      LOG_WARN("Lua mod '%.*s' scheduled callback %lld took %llu us (soft frame budget: %llu us)",
               STR_ARG(context->mod_id),
               (long long)job_handle,
               (unsigned long long)callback_us,
               (unsigned long long)CONFIG_LUA_UE4SS_GAME_DISPATCH_BUDGET_US);
      if (job->active && job->handle == job_handle) {
        job->slow_warned = true;
      }
    }

    if (!job->active || job->handle != job_handle) {
      /* the callback may cancel or replace its own handle */
    } else if ((!called && !(game_thread && job_loop)) || !job_loop || stop_loop) {
      lua_ue4ss_disable_scheduled_job(context, job);
    } else {
      if (called && previous_errors > 0) {
        LOG_INFO("Lua mod '%.*s' scheduled callback %lld recovered after %u failed execution%s",
                 STR_ARG(context->mod_id), (long long)job_handle, previous_errors, previous_errors == 1 ? "" : "s");
        job->consecutive_errors = 0;
        job->last_error_hash    = 0;
      } else if (!called) {
        if (previous_errors == 0) {
          LOG_WARN("Lua mod '%.*s' scheduled callback %lld will suppress repeated retry errors until it succeeds or is cancelled",
                   STR_ARG(context->mod_id), (long long)job_handle);
        }
        job->consecutive_errors = previous_errors < UINT32_MAX ? previous_errors + 1 : UINT32_MAX;
        job->last_error_hash    = callback_error_hash;
      }

      now         = callback_end;
      job->due_us = job->frame_based ? 0 : (job->interval_us > UINT64_MAX - now ? UINT64_MAX : now + job->interval_us);

      uint64_t tick_delay = job->frame_based ? MAX_VAL(job->interval_ticks, 1ULL) : 1ULL;
      job->ready_tick = tick_delay > UINT64_MAX - *scheduler_tick ? UINT64_MAX : *scheduler_tick + tick_delay;
    }

    now = time_now_us();
    uint64_t dispatch_limit = game_thread ? CONFIG_LUA_UE4SS_GAME_DISPATCH_BUDGET_US : CONFIG_LUA_UE4SS_ASYNC_DISPATCH_BUDGET_US;
    if (now - dispatch_start >= dispatch_limit) {
      break;
    }
  }
}

static bool
lua_ue4ss_pending_try_lock(lua_ue4ss_context_t *context)
{
  return context && atomic_i32_compare_exchange(&context->pending_lock, 1, 0) == 0;
}

static bool
lua_ue4ss_pending_lock(lua_ue4ss_context_t *context)
{
  for (int attempt = 0; attempt < 1024; ++attempt) {
    if (lua_ue4ss_pending_try_lock(context)) {
      return true;
    }
  }
  return false;
}

static void
lua_ue4ss_pending_unlock(lua_ue4ss_context_t *context)
{
  if (context) {
    (void)atomic_i32_compare_exchange(&context->pending_lock, 0, 1);
  }
}

void
lua_ue4ss_notify_uobject_constructed(lua_ue4ss_context_t *context, uobject_t *object)
{
  if (!context || !object || !context->pending_objects) {
    return;
  }

  if (!lua_ue4ss_pending_lock(context)) {
    return;
  }

  bool matches = false;
  for (int i = 0; i < context->notification_count; ++i) {
    lua_ue4ss_notification_t *notification = &context->notifications[i];
    if (!notification->active || notification->callback_ref == LUA_NOREF || notification->callback_ref == LUA_REFNIL) {
      continue;
    }

    uclass_t *target = (uclass_t *)unreal_fweak_object_resolve(notification->target_class);
    if (!target || unreal_uobject_is_a(object, target)) {
      /* preserve the dispatch-time class refresh when a registered UClass was unloaded, but do not queue unrelated objects for valid targets */
      matches = true;
      break;
    }
  }

  if (!matches) {
    lua_ue4ss_pending_unlock(context);
    return;
  }

  bool               queued = false;
  fweak_object_ptr_t weak   = {.object_idx = -1};
  if (context->pending_object_count < CONFIG_LUA_UE4SS_MAX_PENDING_OBJECTS && unreal_fweak_object_from_object(object, &weak)) {
    context->pending_objects[context->pending_object_count++].object = weak;
    queued = true;
  } else {
    context->pending_overflow = true;
  }
  lua_ue4ss_pending_unlock(context);

  if (queued && context->async_events && context->wake_async) {
    context->wake_async(context->wake_async_user);
  }
}

static void
lua_ue4ss_dispatch_new_objects(lua_ue4ss_context_t *context)
{
  if (!context || !context->runtime || !context->runtime->inited || context->notification_count <= 0) {
    return;
  }

  lua_ue4ss_pending_object_t pending[CONFIG_LUA_UE4SS_MAX_PENDING_OBJECTS];
  int                        pending_count = 0;
  bool                       overflow      = false;
  if (!lua_ue4ss_pending_lock(context)) {
    return;
  }

  pending_count = context->pending_object_count;
  if (pending_count > 0) {
    mem_copy(pending, context->pending_objects, (uint64_t)pending_count * sizeof(*pending));
  }

  context->pending_object_count = 0;
  overflow                      = context->pending_overflow;
  context->pending_overflow     = false;
  lua_ue4ss_pending_unlock(context);

  if (overflow) {
    LOG_WARN("Lua mod '%.*s' new-object notification queue overflowed; some notifications were dropped", STR_ARG(context->mod_id));
  }

  lua_State *state = context->runtime->state;
  for (int object_idx = 0; object_idx < pending_count; ++object_idx) {
    uobject_t *object = unreal_fweak_object_resolve(pending[object_idx].object);
    if (!object) {
      continue;
    }

    int notification_count = context->notification_count;
    for (int i = 0; i < notification_count; ++i) {
      lua_ue4ss_notification_t *notification = &context->notifications[i];
      if (!notification->active || notification->callback_ref == LUA_NOREF || notification->callback_ref == LUA_REFNIL) {
        continue;
      }

      uclass_t *target = (uclass_t *)unreal_fweak_object_resolve(notification->target_class);
      if (!target) {
        target = unreal_uobject_find_class_by_full_name(notification->class_path);
        if (target) {
          fweak_object_ptr_t target_weak = {.object_idx = -1};
          if (unreal_fweak_object_from_object((uobject_t *)target, &target_weak) && lua_ue4ss_pending_lock(context)) {
            notification->target_class = target_weak;
            lua_ue4ss_pending_unlock(context);
          }
        }
      }

      if (!target || !unreal_uobject_is_a(object, target)) {
        continue;
      }

      int stack_base   = lua_gettop(state);
      int callback_ref = notification->callback_ref;
      if (!lua_unreal_object_push(state, object) || !lua_ue4ss_call_ref(context, callback_ref, 1, 0, CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US, true, 0, NULL)) {
        if (notification->callback_ref == callback_ref) {
          if (lua_ue4ss_pending_lock(context)) {
            lua_ue4ss_disable_callback(context, &notification->callback_ref);
            notification->active = false;
            lua_ue4ss_pending_unlock(context);
          }
        }
      }
      lua_settop(state, stack_base);
    }
  }
}

void
lua_ue4ss_dispatch_game_thread(lua_ue4ss_context_t *context)
{
  if (!context || !context->runtime || !context->runtime->inited) {
    return;
  }

  lua_ue4ss_dispatch_scheduled_jobs(context, true);
  /* a game-thread job queued by a new-object callback must not execute in the same tick.
   * dispatch existing jobs before delivering new notifications. */
  if (!context->async_events) {
    lua_ue4ss_dispatch_new_objects(context);
  }
}

void
lua_ue4ss_dispatch_async(lua_ue4ss_context_t *context)
{
  if (!context || !context->runtime || !context->runtime->inited) {
    return;
  }

  if (context->async_events) {
    lua_ue4ss_dispatch_pending_hooks(context);
    lua_ue4ss_dispatch_new_objects(context);
    lua_ue4ss_dispatch_pending_inputs(context);
    lua_ue4ss_dispatch_pending_commands(context);
  }
  lua_ue4ss_dispatch_scheduled_jobs(context, false);
}

uint32_t
lua_ue4ss_async_wait_ms(lua_ue4ss_context_t *context)
{
  if (!context || !context->scheduled_jobs || context->scheduled_job_count <= 0) {
    return UINT32_MAX;
  }

  uint64_t now          = time_now_us();
  uint64_t earliest_due = UINT64_MAX;
  bool     found        = false;
  for (int i = 0; i < CONFIG_LUA_UE4SS_MAX_SCHEDULED_JOBS; ++i) {
    lua_ue4ss_scheduled_job_t *job = &context->scheduled_jobs[i];
    if (!job->active || job->game_thread) {
      continue;
    }

    found = true;
    if (job->ready_tick > context->async_scheduler_tick) {
      earliest_due = MIN_VAL(earliest_due, job->due_us);
    } else if (job->due_us <= now) {
      return 0;
    } else {
      earliest_due = MIN_VAL(earliest_due, job->due_us);
    }
  }

  if (!found) {
    return UINT32_MAX;
  }

  if (earliest_due <= now) {
    return 0;
  }

  uint64_t wait_ms = time_us_to_ms(earliest_due - now);
  return (uint32_t)MIN_VAL(wait_ms, (uint64_t)(UINT32_MAX - 1));
}

bool
lua_ue4ss_queue_input(lua_ue4ss_context_t *context, input_event_t *ev)
{
  if (!context || !ev || !context->pending_inputs || !lua_ue4ss_pending_lock(context)) {
    return false;
  }

  bool interested = false;
  for (int i = 0; i < context->keybind_count && !interested; ++i) {
    lua_ue4ss_keybind_t *keybind = &context->keybinds[i];
    interested = keybind->callback_ref != LUA_NOREF && keybind->callback_ref != LUA_REFNIL && keybind_activated_by_event(keybind->bind, ev);
  }

  bool queued = false;
  if (interested) {
    if (context->pending_input_count < CONFIG_LUA_UE4SS_MAX_PENDING_INPUTS) {
      context->pending_inputs[context->pending_input_count++] = *ev;
      queued = true;
    } else {
      context->pending_input_overflow = true;
    }
  }
  lua_ue4ss_pending_unlock(context);

  if (queued && context->wake_async) {
    context->wake_async(context->wake_async_user);
  }
  return queued;
}

bool
lua_ue4ss_queue_command(lua_ue4ss_context_t *context, str_t name, str_t args)
{
  if (!context || str_is_empty(name) || !context->pending_commands || !lua_ue4ss_pending_lock(context)) {
    return false;
  }

  bool matched = false;
  for (int i = 0; i < context->command_count; ++i) {
    if (str_equal(context->commands[i].name, name, STR_CMP_FLAG_IGNORE_CASE)) {
      matched = true;
      break;
    }
  }

  lua_ue4ss_pending_unlock(context);
  if (!matched) {
    return false;
  }

  if (name.len > SIZE_MAX - args.len) {
    LOG_ERROR("Lua mod '%.*s' console command is too large to queue", STR_ARG(context->mod_id));
    return true;
  }

  size_t data_size = (size_t)(name.len + args.len);
  char  *data      = malloc(MAX_VAL(data_size, (size_t)1));
  if (!data) {
    LOG_ERROR("Lua mod '%.*s' could not allocate a queued console command", STR_ARG(context->mod_id));
    return true;
  }

  if (name.len > 0) {
    mem_copy(data, name.data, name.len);
  }

  if (args.len > 0) {
    mem_copy(data + name.len, args.data, args.len);
  }

  bool queued = false;
  if (lua_ue4ss_pending_lock(context)) {
    if (context->pending_command_count < CONFIG_LUA_UE4SS_MAX_PENDING_COMMANDS) {
      context->pending_commands[context->pending_command_count++] = (lua_ue4ss_pending_command_t){
        .data     = data,
        .name_len = name.len,
        .args_len = args.len,
      };
      queued = true;
    } else {
      context->pending_command_overflow = true;
    }
    lua_ue4ss_pending_unlock(context);
  }

  if (!queued) {
    free(data);
  } else if (context->wake_async) {
    context->wake_async(context->wake_async_user);
  }

  /* a registered command remains claimed even if its bounded queue overflowed */
  return true;
}

static void
lua_ue4ss_dispatch_pending_inputs(lua_ue4ss_context_t *context)
{
  input_event_t pending[CONFIG_LUA_UE4SS_MAX_PENDING_INPUTS];
  int           pending_count = 0;
  bool          overflow      = false;
  if (!context || !context->pending_inputs || !lua_ue4ss_pending_lock(context)) {
    return;
  }

  pending_count = context->pending_input_count;
  if (pending_count > 0) {
    mem_copy(pending, context->pending_inputs, (uint64_t)pending_count * sizeof(*pending));
  }

  context->pending_input_count    = 0;
  overflow                        = context->pending_input_overflow;
  context->pending_input_overflow = false;
  lua_ue4ss_pending_unlock(context);

  if (overflow) {
    LOG_WARN("Lua mod '%.*s' input queue overflowed; some callbacks were dropped", STR_ARG(context->mod_id));
  }

  for (int i = 0; i < pending_count; ++i) {
    lua_ue4ss_dispatch_keybinds(context, &pending[i]);
  }
}

static void
lua_ue4ss_dispatch_pending_commands(lua_ue4ss_context_t *context)
{
  lua_ue4ss_pending_command_t pending[CONFIG_LUA_UE4SS_MAX_PENDING_COMMANDS];
  int                         pending_count = 0;
  bool                        overflow      = false;
  if (!context || !context->pending_commands || !lua_ue4ss_pending_lock(context)) {
    return;
  }

  pending_count = context->pending_command_count;
  if (pending_count > 0) {
    mem_copy(pending, context->pending_commands, (uint64_t)pending_count * sizeof(*pending));
    mem_zero(context->pending_commands, (uint64_t)pending_count * sizeof(*pending));
  }

  context->pending_command_count    = 0;
  overflow                          = context->pending_command_overflow;
  context->pending_command_overflow = false;
  lua_ue4ss_pending_unlock(context);

  if (overflow) {
    LOG_WARN("Lua mod '%.*s' console command queue overflowed; some callbacks were dropped", STR_ARG(context->mod_id));
  }

  for (int i = 0; i < pending_count; ++i) {
    lua_ue4ss_pending_command_t *command = &pending[i];

    str_t name = str_make(command->data, command->name_len);
    str_t args = str_make(command->data + command->name_len, command->args_len);
    lua_ue4ss_dispatch_command(context, name, args, NULL);
    free(command->data);
  }
}

static void
lua_ue4ss_dispatch_keybinds(lua_ue4ss_context_t *context, input_event_t *ev)
{
  if (!context || !context->runtime || !context->runtime->inited || !ev) {
    return;
  }

  lua_State *state = context->runtime->state;
  int callback_refs[CONFIG_LUA_UE4SS_MAX_KEYBINDS];
  int callback_count = 0;
  if (!lua_ue4ss_pending_lock(context)) {
    return;
  }

  for (int i = 0; i < context->keybind_count; ++i) {
    lua_ue4ss_keybind_t *keybind = &context->keybinds[i];
    if (keybind->callback_ref != LUA_NOREF && keybind->callback_ref != LUA_REFNIL &&
        keybind_activated_by_event(keybind->bind, ev)) {
      callback_refs[callback_count++] = keybind->callback_ref;
    }
  }
  lua_ue4ss_pending_unlock(context);

  for (int i = 0; i < callback_count; ++i) {
    int callback_ref = callback_refs[i];
    lua_settop(state, 0);
    if (!lua_ue4ss_call_ref(context, callback_ref, 0, 0, CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US, true, 0, NULL) && lua_ue4ss_pending_lock(context)) {
      for (int keybind_idx = 0; keybind_idx < context->keybind_count; ++keybind_idx) {
        if (context->keybinds[keybind_idx].callback_ref == callback_ref) {
          lua_ue4ss_disable_callback(context, &context->keybinds[keybind_idx].callback_ref);
          break;
        }
      }
      lua_ue4ss_pending_unlock(context);
    }
    lua_settop(state, 0);
  }
}

bool
lua_ue4ss_dispatch_command(lua_ue4ss_context_t *context, str_t name, str_t args, bool *handled)
{
  if (handled) {
    *handled = false;
  }

  if (!context || !context->runtime || !context->runtime->inited || str_is_empty(name)) {
    return false;
  }

  for (int i = 0; i < context->command_count; ++i) {
    lua_ue4ss_command_t *command = &context->commands[i];
    if (!str_equal(command->name, name, STR_CMP_FLAG_IGNORE_CASE)) {
      continue;
    }

    lua_State *state = context->runtime->state;
    lua_settop(state, 0);

    luaL_Buffer full_command;
    luaL_buffinit(state, &full_command);
    luaL_addlstring(&full_command, (const char *)name.data, (size_t)name.len);
    if (!str_is_empty(args)) {
      luaL_addchar(&full_command, ' ');
      luaL_addlstring(&full_command, (const char *)args.data, (size_t)args.len);
    }

    luaL_pushresult(&full_command);
    lua_ue4ss_push_command_parts(state, args);
    lua_pushnil(state);

    bool called = lua_ue4ss_call_ref(context, command->callback_ref, 3, 1, CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US, true, 0, NULL);
    bool result = called && lua_toboolean(state, -1) != 0;
    lua_settop(state, 0);
    if (handled) {
      *handled = called ? result : true;
    }
    return true;
  }
  return false;
}

static bool
lua_ue4ss_hook_call(lua_ue4ss_context_t *context, lua_ue4ss_hook_t *hook, int *callback_ref_slot, uobject_t *object, ufunc_t *function, void *params)
{
  if (!context || !hook || !callback_ref_slot || !object || !function || *callback_ref_slot == LUA_NOREF || *callback_ref_slot == LUA_REFNIL) {
    return false;
  }

  lua_State *state        = context->runtime->state;
  int        stack_base   = lua_gettop(state);
  int        callback_ref = *callback_ref_slot;
  int        scope_ref    = lua_unreal_param_scope_begin(state);
  int        arg_count    = 0;
  bool       marshalled   = scope_ref != LUA_NOREF && lua_unreal_remote_object_param_push(state, scope_ref, object) != 0;
  if (marshalled) {
    arg_count = 1;
  }

  for (ffield_t *field = function->child_props; field && marshalled; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (!(prop->prop_flags & CPF_PARM) || (prop->prop_flags & CPF_RETURN_PARM)) {
      continue;
    }

    uint64_t complete_size = unreal_fprop_complete_size(prop);
    bool     fits          = params && prop->offset_internal >= 0 && complete_size > 0 &&
                             (uint64_t)prop->offset_internal <= function->params_size &&
                             complete_size <= (uint64_t)function->params_size - (uint64_t)prop->offset_internal;

    void *value    = fits ? unreal_fprop_value_in_container(prop, params, 0) : NULL;
    bool  writable = true;
    if (!value || !lua_unreal_remote_param_push(state, scope_ref, prop, value, object, writable)) {
      marshalled = false;
      break;
    }

    arg_count += 1;
  }

  uint64_t callback_start = time_now_us();
  bool     called         = marshalled && lua_ue4ss_call_ref(context, callback_ref, arg_count, 0, CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US, true, 0, NULL);
  uint64_t callback_end   = time_now_us();
  lua_unreal_param_scope_end(state, scope_ref);
  lua_settop(state, stack_base);

  bool     post        = callback_ref_slot == &hook->post_callback_ref;
  bool    *slow_warned = post ? &hook->post_slow_warned : &hook->pre_slow_warned;
  uint64_t callback_us = callback_end >= callback_start ? callback_end - callback_start : 0;
  if (!context->async_events && marshalled && callback_us > CONFIG_LUA_UE4SS_GAME_DISPATCH_BUDGET_US && !*slow_warned) {
    LOG_WARN("Lua mod '%.*s' %s-hook '%.*s' took %llu us (soft frame budget: %llu us)",
             STR_ARG(context->mod_id),
             post ? "post" : "pre",
             STR_ARG(hook->target),
             (unsigned long long)callback_us,
             (unsigned long long)CONFIG_LUA_UE4SS_GAME_DISPATCH_BUDGET_US);
    *slow_warned = true;
  }

  if (!called && *callback_ref_slot == callback_ref) {
    if (!marshalled) {
      LOG_ERROR("Lua hook '%.*s' has an unsupported or invalid reflected parameter schema", STR_ARG(hook->target));
    }
    lua_ue4ss_disable_callback(context, callback_ref_slot);
  }
  return called;
}

static void
lua_ue4ss_pending_hook_destroy(lua_ue4ss_pending_hook_t *pending)
{
  if (!pending) {
    return;
  }

  if (pending->params && pending->function_raw) {
    uint32_t destroyed = 0;
    for (ffield_t *field = pending->function_raw->child_props; field && destroyed < pending->initialized_count; field = field->next) {
      fprop_t *prop = (fprop_t *)field;
      if (prop->prop_flags & CPF_PARM) {
        unreal_fprop_destroy_in_container(prop, pending->params);
        destroyed += 1;
      }
    }
  }

  free(pending->params);
  *pending = (lua_ue4ss_pending_hook_t){0};
}

static bool
lua_ue4ss_pending_hook_snapshot(lua_ue4ss_pending_hook_t *pending, uobject_t *object, ufunc_t *function, void *params, bool post)
{
  if (!pending || !object || !function || !unreal_fweak_object_from_object(object, &pending->object) ||
      !unreal_fweak_object_from_object((uobject_t *)function, &pending->function)) {
    return false;
  }

  pending->function_raw = function;
  pending->post         = post;
  if (function->params_size == 0) {
    return true;
  }

  if (!params) {
    return false;
  }

  pending->params = malloc((size_t)function->params_size);
  if (!pending->params) {
    return false;
  }
  mem_zero(pending->params, function->params_size);

  for (ffield_t *field = function->child_props; field; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (!(prop->prop_flags & CPF_PARM)) {
      continue;
    }

    void *dst = unreal_fprop_value_in_container(prop, pending->params, 0);
    void *src = unreal_fprop_value_in_container(prop, params, 0);
    if (!dst || !src || !unreal_fprop_initialize_value(prop, dst)) {
      lua_ue4ss_pending_hook_destroy(pending);
      return false;
    }
    pending->initialized_count += 1;

    if (!unreal_fprop_copy_complete_value(prop, dst, src)) {
      lua_ue4ss_pending_hook_destroy(pending);
      return false;
    }
  }
  return true;
}

static bool
lua_ue4ss_queue_process_event(lua_ue4ss_context_t *context, uobject_t *object, ufunc_t *function, void *params, bool post)
{
  if (!context || !context->pending_hooks || !object || !function || !lua_ue4ss_pending_lock(context)) {
    return false;
  }

  bool matches = false;
  for (int i = 0; i < context->hook_count; ++i) {
    lua_ue4ss_hook_t *hook = &context->hooks[i];
    int callback_ref = post ? hook->post_callback_ref : hook->pre_callback_ref;
    if (hook->active && callback_ref != LUA_NOREF && callback_ref != LUA_REFNIL && unreal_fweak_object_resolve(hook->function) == (uobject_t *)function) {
      matches = true;
      break;
    }
  }
  lua_ue4ss_pending_unlock(context);

  if (!matches) {
    return false;
  }

  lua_ue4ss_pending_hook_t pending = {0};
  if (!lua_ue4ss_pending_hook_snapshot(&pending, object, function, params, post)) {
    LOG_ERROR("Lua mod '%.*s' could not snapshot UFunction hook parameters", STR_ARG(context->mod_id));
    return false;
  }

  bool queued = false;
  if (lua_ue4ss_pending_lock(context)) {
    if (context->pending_hook_count < CONFIG_LUA_UE4SS_MAX_PENDING_HOOKS) {
      context->pending_hooks[context->pending_hook_count++] = pending;
      queued = true;
    } else {
      context->pending_hook_overflow = true;
    }
    lua_ue4ss_pending_unlock(context);
  }

  if (!queued) {
    lua_ue4ss_pending_hook_destroy(&pending);
    return false;
  }

  if (context->wake_async) {
    context->wake_async(context->wake_async_user);
  }
  return true;
}

static void
lua_ue4ss_dispatch_pending_hooks(lua_ue4ss_context_t *context)
{
  if (!context || !context->runtime || !context->runtime->inited || !context->pending_hooks) {
    return;
  }

  lua_ue4ss_pending_hook_t pending[CONFIG_LUA_UE4SS_MAX_PENDING_HOOKS];
  int                      pending_count = 0;
  bool                     overflow      = false;
  if (!lua_ue4ss_pending_lock(context)) {
    return;
  }

  pending_count = context->pending_hook_count;
  if (pending_count > 0) {
    mem_copy(pending, context->pending_hooks, (uint64_t)pending_count * sizeof(*pending));
    mem_zero(context->pending_hooks, (uint64_t)pending_count * sizeof(*pending));
  }

  context->pending_hook_count    = 0;
  overflow                       = context->pending_hook_overflow;
  context->pending_hook_overflow = false;
  lua_ue4ss_pending_unlock(context);

  if (overflow) {
    LOG_WARN("Lua mod '%.*s' UFunction hook queue overflowed; some callbacks were dropped", STR_ARG(context->mod_id));
  }

  for (int i = 0; i < pending_count; ++i) {
    lua_ue4ss_pending_hook_t *event    = &pending[i];
    uobject_t                *object   = unreal_fweak_object_resolve(event->object);
    ufunc_t                  *function = (ufunc_t *)unreal_fweak_object_resolve(event->function);
    if (object && function == event->function_raw) {
      lua_ue4ss_dispatch_process_event(context, object, function, event->params, event->post);
    }
    lua_ue4ss_pending_hook_destroy(event);
  }
}

static void
lua_ue4ss_dispatch_process_event(lua_ue4ss_context_t *context, uobject_t *obj, ufunc_t *func, void *params, bool post)
{
  if (!context || !context->runtime || !context->runtime->inited || !obj || !func) {
    return;
  }

  int hook_count = context->hook_count;
  for (int i = 0; i < hook_count; ++i) {
    lua_ue4ss_hook_t *hook = &context->hooks[i];
    if (!hook->active || unreal_fweak_object_resolve(hook->function) != (uobject_t *)func) {
      continue;
    }
    lua_ue4ss_hook_call(context, hook, post ? &hook->post_callback_ref : &hook->pre_callback_ref, obj, func, params);
  }
}

bool
lua_ue4ss_dispatch_process_event_pre(lua_ue4ss_context_t *context, uobject_t *obj, ufunc_t *func, void *params)
{
  if (context && context->async_events) {
    lua_ue4ss_queue_process_event(context, obj, func, params, false);
  } else {
    lua_ue4ss_dispatch_process_event(context, obj, func, params, false);
  }
  return false;
}

void
lua_ue4ss_dispatch_process_event_post(lua_ue4ss_context_t *context, uobject_t *obj, ufunc_t *func, void *params, bool consumed)
{
  (void)consumed;
  if (context && context->async_events) {
    lua_ue4ss_queue_process_event(context, obj, func, params, true);
  } else {
    lua_ue4ss_dispatch_process_event(context, obj, func, params, true);
  }
}
