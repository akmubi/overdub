# Native Overdub Lua

The native runtime is the default for new Lua mods. It provides a small lifecycle, game-thread Unreal access, cooperative tasks, worker jobs, input, and UI.

Each mod owns one Lua 5.4 state. That state belongs to the game thread. It does not receive UE4SS globals; import Overdub APIs with `require`.

## First mod

Create this package:
```
example-mod/
|-- mod.ini
+-- scripts/
    +-- main.lua
```

```ini
[info]
id      = author.example
name    = Example Lua Mod
author  = Author Name
version = 1.0.0
kind    = mod

[lua]
runtime = overdub
```

The entry script is an ordinary Lua module. It returns a table of lifecycle callbacks.
```lua
local overdub = require("overdub")

return {
    Startup = function()
        overdub.Log.Info("Loaded %s", overdub.Name)
    end,

    Tick = function(delta_seconds)
    end,

    Input = function(event)
        return false
    end,

    DrawPanel = function(ctx)
        ctx:Text("Running")
    end,

    DrawConfig = function(ctx)
    end,

    Shutdown = function()
        overdub.Log.Info("Stopped")
    end,
}
```

All fields are optional, but every present field must be a function. The loader executes the chunk, validates the returned table, calls `Startup`, and only then marks the mod active. An entry or startup error closes the state and leaves the mod inactive.

A failing runtime callback is disabled without stopping unrelated parts of the mod. `Shutdown` runs when a successfully started mod stops.

## Core module

`require("overdub")` returns the current mod's identity and common helpers.
```lua
local overdub = require("overdub")

overdub.Log.Info("id=%s frame=%d", overdub.Id, overdub.Frame())
overdub.Log.Debug("game=%s", overdub.GameDir)
overdub.Log.Debug("mods=%s", overdub.RootDir)
overdub.Log.Debug("this mod=%s", overdub.ModDir)
```

`NowUs` returns the runtime's monotonic microsecond clock. It is useful for local measurements.

Console commands belong to the mod and are removed at shutdown:
```lua
local command = overdub.RegisterCommand("example.status", function(args)
    overdub.Log.Info("status args: %s", args)
end)

if command:IsActive() then
    -- command:Remove() can unregister it early.
end
```

## Lua modules

The runtime opens the normal `package`, `io`, and `os` libraries. It prepends the entry-script directory and `ModDir` to `package.path` and `package.cpath`.
```
example-mod/
|-- mod.ini
|-- dkjson.lua
+-- scripts/
    |-- main.lua
    +-- state.lua
```

```lua
local json  = require("dkjson")
local state = require("state")
```

This is also how a mod loads its own Lua 5.4 C module. See [Lua C Modules](c-modules.md).

## Directories and INI files

`ListDirectory` accepts any path and an optional pattern. It returns file records with `Name`, `Path`, `IsDirectory`, and `Size`.
```lua
local entries, message = overdub.ListDirectory(overdub.ModDir .. "/data", "*.json")
if not entries then
    overdub.Log.Error("cannot list data: %s", message)
else
    for _, entry in ipairs(entries) do
        overdub.Log.Info("%s", entry.Path)
    end
end
```

`ReadIni` preserves section order and repeated sections. Each result has `Name`, optional `Argument`, original `Lines`, and parsed `Values`.
```lua
local sections = assert(overdub.ReadIni(overdub.ModDir .. "/mod.ini"))

for _, section in ipairs(sections) do
    if section.Name == "archipelago" then
        local host = section.Values.host
        local slot = section.Values.slot
        connect(host, slot)
    end
end
```

The same helper can read `config.ini` or another INI owned by the mod. Standard Lua file functions remain available when the mod needs another format.

## Input

Polling normally belongs in `Tick`. Key names are Unreal `FKey` names such as `F8`, `SpaceBar`, `LeftMouseButton`, and `Gamepad_FaceButton_Bottom`.
```lua
local input = require("overdub.input")

return {
    Tick = function(delta_seconds)
        if input.Pressed("F8") then
            toggle_feature()
        end

        local left_x = input.Analog("Gamepad_LeftX")
        local mouse_x, mouse_y = input.MousePosition()
    end,

    Input = function(event)
        if event.Kind == "KeyDown" and event.Key == "F9" then
            return true
        end
    end,
}
```

`Down` reports held state. `Pressed` and `Released` report edges for the current frame. `Analog`, `WheelDelta`, and `MousePosition` expose the remaining polled state.

An input event is an immutable snapshot. Its common fields include `Kind`, `Key`, `Pressed`, `Released`, `Repeat`, `Shift`, `Ctrl`, `Alt`, `Keyboard`, `Mouse`, and `Gamepad`. Mouse, character, analog, and activation events add fields for their own values.

Return true from `Input` only when the event should be consumed.

## Work that spans frames

Do not perform a long scan, file parse, or blocking library call inside `Tick`. Use a cooperative task for work that must touch Unreal and a worker job for work that uses only copied Lua data.

The complete examples are in [Tasks and Workers](tasks.md). UI is covered by [Lua UI](ui.md), and reflection is covered by [Lua Unreal](unreal.md).

## Limits and cleanup

The entry script has a 2 ms Lua execution limit. Ordinary Lua callbacks have a 250 us hard limit. Cooperative task code can use its 100 us work-budget hint to choose a yield point. Ready deferred work shares a 500 us soft frame budget and a 1 ms scheduler ceiling.

These limits stop Lua bytecode through an instruction hook. They cannot interrupt code while Lua is inside a blocking C function.

Tasks, worker futures, commands, hooks, delegate bindings, UI windows, notifications, and root claims are removed or cancelled when the mod stops. Keep a handle only when the mod needs to end one of them early.

## Declarations

Use [`Overdub.lua`](Overdub.lua) and the files under [`overdub`](overdub) as LuaLS library files. They describe the native API and should not be executed.
