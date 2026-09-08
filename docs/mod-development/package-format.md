# Package Format

Every mod has one directory under the configured `mods` directory. Only `mod.ini` is required.

```
mods/
+-- author.example/
    |-- mod.ini
    |-- config.ini
    |-- example.dll
    |-- scripts/
    |   +-- main.lua
    +-- assets/
        |-- example.pak
        |-- example.utoc
        +-- example.ucas
```

Overdub discovers mod directories at game startup. Restart the game after adding or removing a package. Asset mounting and mod order also take effect at startup.

## Basic manifest

The `info` section identifies the mod. Keep the ID stable because Overdub uses it for saved order and settings.

```ini
[info]
id      = author.example
name    = Example Mod
author  = Author Name
version = 1.0.0
kind    = mod

[description]
An example Overdub mod.
```

Section names and keys are case-insensitive. A semicolon starts a comment. Unknown sections are left in the file, so a Lua mod can parse its own custom sections with `overdub.ReadIni`.

## Native DLL

The DLL path is relative to the mod directory unless it is absolute.

```ini
[code]
path = example.dll
```

## Lua

Overdub automatically finds `scripts/main.lua` or `main.lua`. Use a `lua` section for another entry path or to select the runtime.

```ini
[lua]
path    = scripts/main.lua
runtime = overdub
```

`overdub` is the default runtime. An existing UE4SS script must opt into the compatibility runtime:

```ini
[lua]
runtime = ue4ss
```

The two runtimes have different lifecycle and public APIs. Do not mix them in one Lua state.

## Assets

Give the common path without an extension. Overdub looks for the matching `.pak`, `.utoc`, and `.ucas` files.

```ini
[assets]
path_without_ext = assets/example
```

A `.utoc` file needs its matching `.ucas` file. A mod may also use only a `.pak` file. Mounting an asset package makes its content available; it does not execute a Blueprint by itself.

## Blueprint actors

A package may have more than one `blueprint` section. Each section describes one generated actor class and the object used as its startup context.

```ini
[blueprint]
id                    = example_actor
mod_actor_class_path  = /Game/Mods/Example/BP_ExampleMod.BP_ExampleMod_C
attach_to             = world
auto_spawn            = true
default_spawn_keybind = F6
```

`attach_to` accepts `none`, `player_controller`, `local_player`, `pawn`, `hud`, `world`, `game_instance`, or `custom_class_path`. A custom context also needs its class path:

```ini
attach_to                = custom_class_path
custom_attach_class_path = Class /Script/GameModule.SomeClass
```

The selected object may not exist in menus or during travel. In that case the actor cannot be spawned yet.

## Saved options

Manifest options appear in the mod manager and are saved to `config.ini`. The supported section arguments are `bool`, `int`, `float`, `enum`, `string`, `keybind`, and `color`.

```ini
[option.bool]
id            = enabled
label         = Enable feature
description   = Turns the feature on.
default_value = true

[option.int]
id            = count
label         = Count
default_value = 3
min_value     = 0
max_value     = 100
step          = 1

[option.enum]
id            = mode
label         = Mode
enum_values   = Off|Normal|Aggressive
default_value = Normal

[option.keybind]
id            = action_key
label         = Action
default_value = F8

[option.color]
id            = display_color
label         = Display color
default_value = 255.128.32.255
```

Integer and float options use `min_value`, `max_value`, and `step`. Enum choices are separated by `|`. Colors use decimal RGBA components.

Lua mods may also read `config.ini` directly when they own custom sections. Native mods normally use the typed configuration handles described in [Native SDK](native-sdk.md).
