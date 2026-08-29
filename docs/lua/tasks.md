# Tasks and Workers

`overdub.task` keeps long work out of the per-frame `Tick` callback. It has two different tools because Unreal work and background work have different safety rules.

A Task is a game-thread Lua coroutine. A Future is a handle to one worker result, similar to a JavaScript Promise.

## Cooperative tasks

Use a Task when the work must read or write UObjects. The task still runs on the game thread, but it can pause and continue in another frame.

```lua
local input = require("overdub.input")
local task = require("overdub.task")
local unreal = require("overdub.unreal")

local scan_task

local function scan_objects()
    for _, object in unreal.Objects() do
        inspect_object(object)

        if task.ShouldYield() then
            task.YieldFrame()
        end
    end
end

return {
    Tick = function(delta_seconds)
        if input.Pressed("F8") and (not scan_task or scan_task:IsDone()) then
            scan_task = task.Spawn("scan-objects", scan_objects)
        end
    end,
}
```

Every call to `Spawn` creates a new task. The condition in `Tick` prevents a second scan while the first one is active. Overdub does not search for an older task by name.

The optional name is only for diagnostics. These calls are equivalent apart from that label:

```lua
task.Spawn(run_scan)
task.Spawn("scan-objects", run_scan)
```

`ShouldYield` compares the time used by the current resume with its work-budget hint. It does not yield by itself and it does not accept a budget argument. Call `YieldFrame` at a safe boundary chosen by the algorithm.

This keeps the decision inside the loop instead of using an arbitrary item count such as `index % 20 == 0`. The amount of work per object can change, while elapsed time still describes the frame cost.

`Sleep` pauses the task until a later frame after the requested time:

```lua
task.Spawn(function()
    show_message("Connected")
    task.Sleep(2.0)
    hide_message()
end)
```

`Task:Cancel` requests early cancellation. `IsDone` becomes true after success, error, or cancellation. All tasks are cancelled when the mod stops.

## Worker jobs

Use `Offload` for CPU work that uses only copied Lua values. The worker cannot access UObjects, the UI context, or closures from the game-thread state.

```lua
local task = require("overdub.task")

local parse_future
local parsed_data

return {
    Tick = function(delta_seconds)
        if reload_requested() and (not parse_future or parse_future:IsDone()) then
            local text = read_text_snapshot()
            parse_future = task.Offload("workers.parser", "Parse", text)

            parse_future:OnSuccess(function(result)
                parsed_data = result
            end)

            parse_future:OnError(function(message)
                print("parse failed: " .. message)
            end)
        end

        if parsed_data then
            apply_small_update(parsed_data)
            parsed_data = nil
        end
    end,
}
```

`Offload` always submits a new job and returns a new Future. It does not know about `parse_future`. The condition belongs to the mod and prevents `Tick` from submitting the same request every frame.

The worker entry is a normal Lua module:

```lua
-- scripts/workers/parser.lua
local parser = {}

function parser.Parse(text)
    local result = expensive_parse(text)
    return {
        entries = result,
        count = #result,
    }
end

return parser
```

`OnSuccess` registers a callback for a result. `OnError` registers a callback for an error or cancellation. `OnComplete` always runs and receives the Future. The names describe registration; they are not switch cases.

The Future marks itself complete. There is no need to assign nil after completion. Keep the handle when `Tick` needs `IsDone` as a resubmission guard, or let Lua collect it when the result is no longer relevant.

## Await a Future

`Await` pauses a cooperative Task. It never blocks `Tick`.

```lua
local rebuild_task

return {
    Tick = function(delta_seconds)
        if needs_rebuild() and (not rebuild_task or rebuild_task:IsDone()) then
            local input_data = make_plain_snapshot()

            rebuild_task = task.Spawn("rebuild", function()
                local future = task.Offload("workers.cache", "Build", input_data)
                local result = future:Await()

                -- This code resumes on the game thread.
                apply_result_to_unreal(result)
            end)
        end
    end,
}
```

Do not call `Await` directly from `Tick`. Tick is a normal callback, not a yieldable coroutine. Await returns the result on success and raises a Lua error after worker failure or cancellation.

`GetResult` reads an already completed successful result. It raises while pending, after failure, or after cancellation. `GetError` returns the failure message, `"cancelled"`, or nil.

## Worker boundary

The boundary supports nil, booleans, integers, numbers, binary-safe strings, arrays, and tables with string or integer keys. Values are serialized and copied.

Cyclic tables, metatables, userdata, functions, coroutines, Lua states, UObjects, and UI values cannot cross the boundary. Capture the needed Unreal data into a plain table on the game thread, then offload that table.

Each mod gets a persistent worker Lua state when it first calls `Offload`. Its loaded modules and module-local state survive between jobs. Jobs for one mod run in order rather than concurrently.

The worker has normal Lua module paths and can load vendored Lua or C modules. It does not provide `overdub`, `overdub.unreal`, or the UI package.

Cancellation settles the Future and discards a late result. It cannot interrupt a third-party C function that is already blocked inside the worker. Mod shutdown cancels outstanding jobs and waits for the worker to finish.
