#include "lua_overdub_input.h"

#include "globals.h"
#include "lua_overdub.h"
#include "str.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <string.h>
#include <windows.h>

#define LUA_OVERDUB_INPUT_EVENT_META "overdub.input.Event"

typedef struct lua_overdub_input_event_s lua_overdub_input_event_t;
struct lua_overdub_input_event_s {
  input_event_t value;
};

static input_key_kind_t
lua_overdub_input_check_key(lua_State *state, int idx)
{
  size_t      name_len  = 0;
  const char *name_data = luaL_checklstring(state, idx, &name_len);
  str_t       name      = str_make((void *)name_data, (uint64_t)name_len);
  input_key_kind_t key  = input_key_from_str(name);

  if (key == INPUT_KEY_NONE) {
    keybind_t bind = keybind_parse(name, KEYBIND_NULL);
    if (bind.count == 1) {
      key = bind.keys[0];
    }
  }

  if (key == INPUT_KEY_NONE) {
    luaL_error(state, "unknown input key '%.*s'", (int)name.len, name.data);
  }
  return key;
}

static int
lua_overdub_input_down(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushboolean(state, input_key_is_down(lua_overdub_input_check_key(state, 1)));
  return 1;
}

static int
lua_overdub_input_pressed(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushboolean(state, input_key_is_pressed(lua_overdub_input_check_key(state, 1)));
  return 1;
}

static int
lua_overdub_input_released(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushboolean(state, input_key_is_released(lua_overdub_input_check_key(state, 1)));
  return 1;
}

static int
lua_overdub_input_analog(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushnumber(state, (lua_Number)input_key_get_analog_value(lua_overdub_input_check_key(state, 1)));
  return 1;
}

static int
lua_overdub_input_wheel_delta(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_pushnumber(state, (lua_Number)globals.wheel_delta);
  return 1;
}

static int
lua_overdub_input_mouse_position(lua_State *state)
{
  lua_overdub_context_check(state);
  POINT position = {0};
  GetCursorPos(&position);
  if (globals.hwnd) {
    ScreenToClient((HWND)globals.hwnd, &position);
  }
  lua_pushinteger(state, (lua_Integer)position.x);
  lua_pushinteger(state, (lua_Integer)position.y);
  return 2;
}

static const char *
lua_overdub_input_event_kind(input_event_kind_t kind)
{
  switch (kind) {
    case INPUT_EVENT_KEY_DOWN:       return "KeyDown";
    case INPUT_EVENT_KEY_UP:         return "KeyUp";
    case INPUT_EVENT_KEY_CHAR:       return "Character";
    case INPUT_EVENT_MOUSE_DOWN:     return "MouseDown";
    case INPUT_EVENT_MOUSE_UP:       return "MouseUp";
    case INPUT_EVENT_MOUSE_DBLCLICK: return "MouseDoubleClick";
    case INPUT_EVENT_MOUSE_MOVE:     return "MouseMove";
    case INPUT_EVENT_MOUSE_WHEEL:    return "MouseWheel";
    case INPUT_EVENT_ANALOG:         return "Analog";
    case INPUT_EVENT_APP_ACTIVATION: return "AppActivation";
  }
  return "Unknown";
}

static int
lua_overdub_input_event_index(lua_State *state)
{
  lua_overdub_input_event_t *event = luaL_checkudata(state, 1, LUA_OVERDUB_INPUT_EVENT_META);
  const char                *field = luaL_checkstring(state, 2);
  input_event_t             *value = &event->value;

  if (strcmp(field, "Kind") == 0) {
    lua_pushstring(state, lua_overdub_input_event_kind(value->kind));
  } else if (strcmp(field, "Key") == 0) {
    str_t key = input_key_to_str(value->key);
    if (str_is_empty(key)) {
      lua_pushnil(state);
    } else {
      lua_pushlstring(state, (const char *)key.data, (size_t)key.len);
    }
  } else if (strcmp(field, "Pressed") == 0) {
    lua_pushboolean(state, value->kind == INPUT_EVENT_KEY_DOWN || value->kind == INPUT_EVENT_MOUSE_DOWN || value->kind == INPUT_EVENT_MOUSE_DBLCLICK);
  } else if (strcmp(field, "Released") == 0) {
    lua_pushboolean(state, value->kind == INPUT_EVENT_KEY_UP || value->kind == INPUT_EVENT_MOUSE_UP);
  } else if (strcmp(field, "Repeat") == 0) {
    lua_pushboolean(state, value->is_repeat);
  } else if (strcmp(field, "Shift") == 0) {
    lua_pushboolean(state, (value->modifiers & INPUT_MOD_SHIFT) != 0);
  } else if (strcmp(field, "Ctrl") == 0) {
    lua_pushboolean(state, (value->modifiers & INPUT_MOD_CTRL) != 0);
  } else if (strcmp(field, "Alt") == 0) {
    lua_pushboolean(state, (value->modifiers & INPUT_MOD_ALT) != 0);
  } else if (strcmp(field, "Keyboard") == 0) {
    lua_pushboolean(state, input_event_is_keyboard(value));
  } else if (strcmp(field, "Mouse") == 0) {
    lua_pushboolean(state, input_event_is_mouse(value));
  } else if (strcmp(field, "Gamepad") == 0) {
    lua_pushboolean(state, input_event_is_gamepad(value));
  } else if (strcmp(field, "Character") == 0 && value->kind == INPUT_EVENT_KEY_CHAR) {
    lua_pushinteger(state, (lua_Integer)value->character);
  } else if (strcmp(field, "Activated") == 0 && value->kind == INPUT_EVENT_APP_ACTIVATION) {
    lua_pushboolean(state, value->app_activated);
  } else if (strcmp(field, "ScreenX") == 0 && input_event_is_mouse(value)) {
    lua_pushinteger(state, (lua_Integer)value->screen_x);
  } else if (strcmp(field, "ScreenY") == 0 && input_event_is_mouse(value)) {
    lua_pushinteger(state, (lua_Integer)value->screen_y);
  } else if (strcmp(field, "ClientX") == 0 && input_event_is_mouse(value)) {
    lua_pushinteger(state, (lua_Integer)value->client_x);
  } else if (strcmp(field, "ClientY") == 0 && input_event_is_mouse(value)) {
    lua_pushinteger(state, (lua_Integer)value->client_y);
  } else if (strcmp(field, "DeltaX") == 0 && value->kind == INPUT_EVENT_MOUSE_MOVE) {
    lua_pushnumber(state, (lua_Number)value->delta_x);
  } else if (strcmp(field, "DeltaY") == 0 && value->kind == INPUT_EVENT_MOUSE_MOVE) {
    lua_pushnumber(state, (lua_Number)value->delta_y);
  } else if (strcmp(field, "WheelDelta") == 0 && value->kind == INPUT_EVENT_MOUSE_WHEEL) {
    lua_pushnumber(state, (lua_Number)value->wheel_delta);
  } else if (strcmp(field, "AnalogValue") == 0 && value->kind == INPUT_EVENT_ANALOG) {
    lua_pushnumber(state, (lua_Number)value->analog_value);
  } else {
    lua_pushnil(state);
  }
  return 1;
}

static int
luaopen_overdub_input(lua_State *state)
{
  lua_overdub_context_check(state);
  static const luaL_Reg functions[] = {
    {"Down",          lua_overdub_input_down          },
    {"Pressed",       lua_overdub_input_pressed       },
    {"Released",      lua_overdub_input_released      },
    {"Analog",        lua_overdub_input_analog        },
    {"WheelDelta",    lua_overdub_input_wheel_delta   },
    {"MousePosition", lua_overdub_input_mouse_position},
    {NULL,            NULL                            },
  };
  luaL_newlib(state, functions);
  return 1;
}

bool
lua_overdub_input_register(lua_State *state)
{
  if (!state) {
    return false;
  }

  if (luaL_newmetatable(state, LUA_OVERDUB_INPUT_EVENT_META)) {
    lua_pushcfunction(state, lua_overdub_input_event_index);
    lua_setfield(state, -2, "__index");
    lua_pushliteral(state, "immutable Overdub input event");
    lua_setfield(state, -2, "__metatable");
  }

  lua_pop(state, 1);
  return lua_overdub_preload_module(state, "overdub.input", luaopen_overdub_input);
}

bool
lua_overdub_input_push_event(lua_State *state, const input_event_t *event)
{
  if (!state || !event) {
    return false;
  }

  lua_overdub_input_event_t *value = lua_newuserdatauv(state, sizeof(*value), 0);
  value->value = *event;
  luaL_setmetatable(state, LUA_OVERDUB_INPUT_EVENT_META);
  return true;
}
