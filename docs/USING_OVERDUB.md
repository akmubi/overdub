# Using Overdub

Overdub opens its windows on top of the game. The default shortcuts are backtick or holding `Select` for the mod manager, `F1` or `Select+RS` for the console, `F2` for UObject Search, and `F3` for UFunction Tracer. The shortcuts can be changed in Settings.

## Install a mod

Extract a mod into its own directory under `mods`. The directory must contain `mod.ini`.

```
mods/
+-- author.example/
    |-- mod.ini
    |-- config.ini
    |-- example.dll
    |-- scripts/
    |   +-- main.lua
    +-- assets/
```

Restart the game after adding or removing a mod. Overdub discovers packages and mounts assets during startup.

## Mod manager

Select a mod on the left to see its status and controls. A green status means that at least one part is active. Gray means inactive. Red means that something failed. Yellow means that a shortcut of the mod has no key or uses the same keys as another one; the details are listed under State.

![Selected mod details](images/mod-details.png)

Enable and Disable control the complete mod. Native DLL and Lua code can be stopped while the game is running. Mounted assets cannot be safely removed, so asset changes still require a restart.

The Code section can start, stop, restart, or reload a native DLL. Reload is intended for development. It is safe only when the mod releases its hooks, callbacks, threads, and other external state before the DLL is unloaded.

The Config section edits options declared by a mod. Save writes the values to `config.ini`. Discard restores the last saved values. Restore defaults changes the current values to their defaults; use Save if those values should remain after restart.

Blueprint entries can be spawned or despawned from their section. A spawn can fail while the required world, pawn, controller, or other context does not exist.

## Mod order

Use Reorder mods to change startup order and asset priority, then restart the game. Runtime callbacks are processed in mod order. When asset packages conflict, a package later in the order normally has priority.

![Reorder mods](images/reorder-mods.png)

## Settings

Settings controls the mod directory, window shortcuts, console behavior, and other loader options. Press Apply to save changes. Changing the mod directory requires a restart.

![Overdub settings](images/settings.png)

## Shortcuts

A shortcut has a keyboard part and a gamepad part, written as `F1 | Select+RS`. Either part can be empty. Overdub's shortcuts are set in Settings, and a mod's shortcuts in its Config section.

A gamepad combination is pressed in its order, its first button first: `Select` before `RS`. It works as soon as all of its buttons are down. Buttons such as `Select` and `Start` wait briefly in case a combination follows; the buttons the game needs on the beat (attacks, dodge, jump) reach it at once. A single-button shortcut works when held for the hold time set in Settings, and a shorter press still reaches the game.

To change a shortcut, open its dialog: opened with the gamepad it sets the gamepad part, otherwise the keyboard part. Choose Capture, press the new keys or buttons together and release them, then Apply. Clear empties the part, and Cancel (or `B`, `Escape`) keeps the shortcut as it was. The dialog warns when the keys are already used by another shortcut.

## Keyboard and gamepad navigation

Overdub's windows can be used without a mouse. The focused widget is outlined, and the outline turns amber while a value or field is being edited. Moving the mouse gives control back to it.

| Action                          | Keyboard        | Gamepad     |
| ------------------------------- | --------------- | ----------- |
| Move between widgets            | Arrows          | D-pad       |
| Press, or start editing a value | Enter           | A           |
| Leave a field, close a popup    | Escape          | B           |
| Context menu                    |                 | X           |
| On-screen keyboard              |                 | Y           |
| Switch window                   | Alt+arrows      | LB / RB     |
| Window menu                     | Alt+W           | Start       |
| Scroll                          | Ctrl+arrows     | Right stick |

Holding a direction on a value changes it faster after a second. While a window is open, the gamepad controls Overdub, except the left stick, which still moves the character. On the on-screen keyboard:
- `LB`/`RB` - move the text cursor
- `LT`/`RT` - move the text cursor by words
- `X`       - delete a symbol

## Console

The console shows messages from Overdub and mods. Commands registered by Overdub or a mod start with a colon.
```
:help
```

The `unreal` command sends the remaining text to Unreal's console command paths and prints captured text when the engine produces any:
```
:unreal stat fps
```

Some Unreal commands change state without printing output. Commands, console variables, and older `FExec` handlers do not all use the same engine path, so availability depends on the game build.

The UI console contains only the current session. Use `overdub.log` for startup errors and older messages.

![Overdub console](images/console.png)

## UObject Search

UObject Search inspects objects that are currently loaded. Search by name or type, select a result, and inspect its reflected properties and functions.

UFunction pages can open a call dialog. The dialog validates the target and input types, initializes reflected parameters, calls the function synchronously, and shows return and output values. This does not make arbitrary game functions safe. A function may change game state, destroy objects, or depend on context that the tool cannot infer.

Ctrl+Tab and Ctrl+Shift+Tab switch the detail tabs, and Ctrl+W closes one.

![UObject Search](images/uobject-search.png)

## UFunction Tracer

UFunction Tracer records reflected calls that match its filters. Start with a narrow filter, begin capture, perform the action in game, and stop capture before inspecting the result.
```
+func:OnBeat
-func:Tick
```

An include rule starts with `+`. An exclude rule starts with `-`. Broad tracing can create a large amount of work and may slow the game.

![UFunction Tracer](images/ufunction-tracer.png)
