# UE4SS Compatibility Runtime

The UE4SS runtime exists for older Lua mods that use UE4SS names and globals. It is separate from native Overdub Lua and must be selected in `mod.ini`.
```ini
[lua]
path    = scripts/main.lua
runtime = ue4ss
```

Do not require `overdub`, `overdub.task`, `overdub.ui`, or `overdub.unreal` from this runtime. It keeps the UE4SS-compatible public names instead.

## State and modules

Each compatible mod has its own Lua state and worker thread. Its entry script runs once and the state remains alive until the mod stops.

The standard `require`, `package`, `io`, and `os` libraries are available. Module lookup includes the entry directory, the mod's `scripts` directory, `mods/shared`, and native module locations used by the compatibility layer.

## Scheduling

The compatibility globals include `ExecuteInGameThread`, delayed game-thread work, `ExecuteAsync`, `ExecuteWithDelay`, `LoopAsync`, frame loops, and cancellation.

```lua
ExecuteInGameThread(function()
    local player = FindFirstOf("PlayerController")
end)

local loop = LoopInGameThreadAfterFrames(1, function()
    update_feature()
end)

CancelDelayedAction(loop)
```

Async callbacks run on the mod's worker thread. Use the game-thread helpers for UObject work.

## Keybinds and commands

```lua
RegisterKeyBind(Key.ONE, { ModifierKey.CONTROL }, function()
    print("Ctrl+1")
end)

RegisterConsoleCommandHandler("hello", function(full_command, parameters, output_device)
    print(parameters[1] or "world")
    return true
end)
```

Console handlers receive commands entered as `:name arguments` in Overdub's console. `output_device` is nil because the UI console does not use Unreal's `FOutputDevice`.

## Reflected hooks

```lua
local pre_id, post_id = RegisterHook(
    "/Script/Engine.Actor:K2_DestroyActor",
    function(self)
        print(self:Get():GetFullName())
    end,
    function(self)
        print("destroy completed")
    end
)

UnregisterHook("/Script/Engine.Actor:K2_DestroyActor", pre_id, post_id)
```

Compatibility hooks receive copied remote parameters. Changing a copy does not alter the original `ProcessEvent` call. Native Overdub hooks are synchronous when possible and have different behavior.

## Unreal data

The compatibility layer includes UObject lookup, reflected property values, UFunction calls, new-object notifications, FProperty metadata, and UDataTable helpers. Existing UE4SS function and type names remain unchanged.

Use [`Types.lua`](Types.lua) as a LuaLS library file for the complete supported surface. It is metadata and must not be executed with `require` or `dofile`.
