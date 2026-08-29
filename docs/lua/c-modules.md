# Lua C Modules

Overdub uses ordinary Lua 5.4 C modules for native interoperation. There is no separate typed-call or signature-scanning Lua package. A C module can express the real C structures, calling convention, hooks, and cleanup rules without another foreign-function type system.

Pure Lua dependencies need no special support. Place a vendored file such as `dkjson.lua` in the mod directory and load it with `require("dkjson")`.

## Small C module

Include `overdub_lua.h`. It provides the Lua headers and the export macro.

```c
#include "overdub_lua.h"

static int
math_add(lua_State *state)
{
  lua_Integer left  = luaL_checkinteger(state, 1);
  lua_Integer right = luaL_checkinteger(state, 2);

  lua_pushinteger(state, left + right);
  return 1;
}

OVERDUB_LUA_MODULE int
luaopen_example_native(lua_State *state)
{
  static const luaL_Reg functions[] = {
    {"Add", math_add},
    {NULL,  NULL    },
  };

  luaL_newlib(state, functions);
  return 1;
}
```

Build it as `example_native.dll`, place it beside `main.lua` or in the mod directory, and require it by the matching export name:

```lua
local native = require("example_native")
print(native.Add(20, 22))
```

The DLL links against the same `overdub.lib` as a native mod.

## Expose a C structure

Use full userdata when Lua should own a C value. The metatable defines its methods and finalizer.

```c
#include "overdub_lua.h"

#define COUNTER_META "example.Counter"

typedef struct counter_s counter_t;
struct counter_s {
  int value;
};

static counter_t *
counter_check(lua_State *state, int index)
{
  return luaL_checkudata(state, index, COUNTER_META);
}

static int
counter_new(lua_State *state)
{
  counter_t *counter = lua_newuserdatauv(state, sizeof(*counter), 0);
  counter->value = (int)luaL_optinteger(state, 1, 0);
  luaL_setmetatable(state, COUNTER_META);
  return 1;
}

static int
counter_add(lua_State *state)
{
  counter_t *counter = counter_check(state, 1);
  counter->value += (int)luaL_checkinteger(state, 2);
  lua_pushinteger(state, counter->value);
  return 1;
}

OVERDUB_LUA_MODULE int
luaopen_example_counter(lua_State *state)
{
  if (luaL_newmetatable(state, COUNTER_META)) {
    static const luaL_Reg methods[] = {
      {"Add", counter_add},
      {NULL,  NULL       },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
  }
  lua_pop(state, 1);

  lua_newtable(state);
  lua_pushcfunction(state, counter_new);
  lua_setfield(state, -2, "New");
  return 1;
}
```

```lua
local counter = require("example_counter").New(10)
print(counter:Add(5))
```

Use `__gc` when userdata owns memory, handles, or library objects. An explicit `Close` method is also useful when cleanup should happen before Lua garbage collection.

## Existing native libraries

A C module is the intended bridge for a library such as an Archipelago client DLL. Keep the library's native details inside C and expose a small Lua-facing object.

```lua
local archipelago = require("hbk_archipelago")

local client

return {
    Startup = function()
        client = assert(archipelago.New({
            host = "archipelago.gg:38281",
            slot = "Chai",
            password = "",
        }))
    end,

    Tick = function(delta_seconds)
        for _, event in ipairs(client:Poll()) do
            handle_archipelago_event(event)
        end
    end,

    Shutdown = function()
        if client then
            client:Close()
            client = nil
        end
    end,
}
```

`Poll` should return quickly. The C library may do socket work on its own thread, place copied events in a queue, and let Lua drain a bounded number from `Tick`. It must not call the game-thread Lua state from its network thread.

This pattern does not require Overdub to provide networking or JSON. The mod chooses its native client and can vendor a Lua JSON parser when needed.

## Game-thread and worker states

A module loaded by the main mod state runs its Lua functions on the game thread. Keep those functions bounded. A blocking C call cannot be stopped by Lua's execution hook.

A worker state can require the same module when the DLL supports worker-thread use. Lua values and userdata still cannot cross from that state to the game-thread state. Return serializable values from the worker function.

Windows loads one DLL image into the process, so C static variables may be shared by several Lua states even though Lua userdata is not. Protect shared native state when a module is used by both the game thread and a worker.

## Lifetime

An ordinary C module does not receive the native mod callback table. Its lifecycle comes from the Lua values it creates and the Lua mod's `Startup` and `Shutdown` callbacks.

Close threads, sockets, hooks, and other external resources explicitly during `Shutdown`. Use userdata finalizers as a fallback, not as the only timely shutdown path. No background thread may call Lua after its state starts closing.
