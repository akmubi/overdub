# Lua UI

`overdub.ui` wraps the Nuklear UI used by Overdub. It exposes ordinary controls and layout without exposing the raw context pointer.

Lua UI can be drawn in three places. `DrawPanel` draws the mod's main panel. `DrawConfig` draws its Config section. `RegisterWindow` creates a separate window. All three use the same callback-scoped context.

## Panel and config callbacks

```lua
local enabled = true
local speed = 1.0

return {
    DrawPanel = function(ctx)
        ctx:Text("Feature status")
        ctx:Text(enabled and "Enabled" or "Disabled")
    end,

    DrawConfig = function(ctx)
        enabled = ctx:Checkbox("Enabled", enabled)
        speed = ctx:SliderFloat("Speed", speed, 0.1, 4.0, 0.1)
    end,
}
```

When `DrawConfig` exists, it replaces the normal manifest-backed controls in that section.

## Separate windows

Register a window during startup and store its state in normal Lua variables.

```lua
local ui = require("overdub.ui")

local status_window
local connected = false
local player_name = "Player"

return {
    Startup = function()
        status_window = ui.RegisterWindow("status", "Connection", function(ctx)
            ctx:Text(connected and "Connected" or "Disconnected")
            ctx:Separator()

            connected = ctx:Checkbox("Connected", connected)
            player_name = ctx:InputText("Name", player_name, 64)

            ctx:Row(2)
            if ctx:Button("Reconnect") then
                reconnect()
            end

            if ctx:Button("Close") then
                status_window:SetOpen(false)
            end
        end)
    end,
}
```

Window IDs are scoped to the mod. A handle can use `SetOpen`, `IsOpen`, `Remove`, and `IsActive`. Windows are removed automatically when the mod stops.

## Rows and grids

Simple widgets create a one-column row when no layout was selected. Use `Row`, `RowStatic`, `RowRatios`, or `RowWidths` for explicit layout.

```lua
ctx:Row(2, 28)
ctx:Button("Left")
ctx:Button("Right")

ctx:RowRatios({ 0.25, 0.75 }, 24)
ctx:Text("Name")
ctx:Text(player_name)

ctx:RowWidths({ 100, 240 }, 24)
ctx:Text("Status")
ctx:Text("Ready")
```

A grid remembers its column layout while the callback runs. Use `NextRow` to advance explicitly.

```lua
ctx:Grid("stats", { "Content", 90, "Flex" }, function(grid)
    grid:Text("Name")
    grid:Text("Value")
    grid:Text("Notes")
    grid:NextRow()

    grid:Text("Score")
    grid:Text("1200")
    grid:Text("Current run")
end)
```

`"Content"` sizes a column to its content. `"Flex"` shares the remaining width.

## Groups and trees

A group creates a clipped region with its own scroll position.

```lua
ctx:Group("items", 180, function(group)
    for _, item in ipairs(items) do
        group:Text(item.Name)
    end
end)
```

`Group` returns its current horizontal and vertical scroll offsets. `GroupScroll` reads them without drawing the group. `SetGroupScroll` changes them when the mod needs to scroll to a saved position or jump to a result.

A clipped group may skip its callback because none of its content is visible. Do not put non-UI game logic inside a group callback.

Trees keep their expanded state in the value returned to the mod:

```lua
tree_open = ctx:Tree("Advanced", tree_open, function(tree)
    tree:Text("Advanced settings")
end)
```

## Inputs and helpers

Buttons and checkboxes return their new state directly. Sliders return the clamped value. `InputInt`, `InputFloat`, and `InputText` return the value plus change information where applicable. `Dropdown` selects from a string array. `ColorEditor` edits RGBA values, and `Keybind` captures an Overdub keybind.

Text helpers include `Text`, `TextColored`, `WrappedText`, and `WrappedTextColored`. Alignment strings are `"Left"`, `"Center"`, and `"Right"`. The remaining helpers are `Tooltip`, `Separator`, `Spacing`, `GroupScroll`, and `ViewportSize`.

See [`ui.lua`](overdub/ui.lua) for exact signatures and return values.

## Context lifetime

The context is valid only during its draw callback. Do not store it in a global, task, Future callback, or worker input.

UI callbacks run on the game thread. They should draw controls and update small pieces of state, not perform searches or file work. A failing separate window removes that window. A failing lifecycle draw callback disables that callback.
