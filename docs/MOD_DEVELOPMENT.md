# Overdub Mod Development Guide

This guide covers the parts of Unreal Engine and Overdub that are directly useful when making a mod for Hi-Fi RUSH. It assumes basic C/C++ knowledge.
Hi-Fi RUSH uses a modified Unreal Engine 4.27 build. Public UE 4.27 source is useful for reference, but always verify addresses, layouts, and behavior against the game build you support.

The public SDK is in [`mod/`](../mod/). A mod should not depend on private loader structures in `src/` or `include/`.

## 1. Choose a Mod Type

An Overdub package may contain any combination of these parts.

| Type       | Use it for                                                           |
|------------|----------------------------------------------------------------------|
| Asset      | Replacing textures, audio, etc.                                      |
| Blueprint  | Gameplay logic made in Unreal Editor that can run as a spawned actor |
| Native DLL | Hooks, custom UI, etc.                                               |

Native DLL mods have the most control and the greatest crash risk. Prefer assets or Blueprints when they can do the job cleanly.

## 2. Package Layout

Every mod has its own directory under `mods/`.
```
mods/
+-- example-mod/
    +---assets/
    |   |-- example.pak
    |   |-- example.utoc
    |   +-- example.ucas
    |-- mod.ini
    |-- config.ini
    +-- example.dll
```

Only `mod.ini` is required. Include only the files the mod uses.  The mod list and asset mount order are created at startup. Restart the game after adding or removing a mod, changing asset files, or changing mod order.

### Minimal native mod manifest
```ini
[info]
id      = author.example
name    = Example Mod
author  = Author Name
version = 1.0.0
kind    = mod

[description]
Example native mod.

[code]
path = example.dll
```

Keep `id` stable between releases. It is used for saved order and configuration.

### Asset files
```ini
[assets]
path_without_ext = example
```

Overdub looks for:
```
example.pak
example.utoc
example.ucas
```

The `.utoc` and `.ucas` files are a pair. A package may also contain only a `.pak`.

### Blueprint actor
```ini
[blueprint]
id                    = example_actor
mod_actor_class_path  = /Game/Mods/Example/BP_ExampleMod.BP_ExampleMod_C
attach_to             = world
auto_spawn            = true
default_spawn_keybind = F6
```

Accepted `attach_to` values:
- `none`
- `player_controller`
- `local_player`
- `pawn`
- `hud`
- `world`
- `game_instance`
- `custom_class_path`

For a custom context:
```ini
attach_to                = custom_class_path
custom_attach_class_path = Class /Script/GameModule.SomeClass
```

The selected context must exist when the actor is spawned. A pawn, HUD, or player controller may be absent on menus and loading screens.

### Options

Supported sections:
```
[option.bool]
[option.int]
[option.float]
[option.enum]
[option.string]
[option.keybind]
[option.color]
```

Common keys:
```ini
[option.bool]
id            = feature_enabled
label         = Enable feature
description   = Enables the feature.
default_value = true
```

Integer and float options also support `min_value`, `max_value`, and `step`.
```ini
[option.int]
id            = count
label         = Count
default_value = 3
min_value     = 0
max_value     = 100
step          = 1
```

Enums use `|` separators:
```ini
[option.enum]
id            = mode
label         = Mode
enum_values   = Off|Normal|Aggressive
default_value = Normal
```

Colors use decimal RGBA:
```ini
[option.color]
id            = display_color
label         = Display color
default_value = 255.128.32.255
```

Section names and keys are case-insensitive. A semicolon starts a comment.

## 3. Building a Native Mod

The included Visual Studio project targets Visual Studio 2022, x64, C11 or C++20, and the static MSVC runtime.

Debug build:
```batch
msbuild mod\mod.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:ModName=example
```

Release build:
```batch
msbuild mod\mod.vcxproj /p:Configuration=Release /p:Platform=x64 /p:ModName=example
```

Output:
```
mod/build/<Configuration>/example.dll
```

A separate project should compile the matching SDK files from `mod/`.

## 4. Minimal Native Mod

```c
#include "mod_sdk.h"

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  MOD_LOG_INFO("Example mod started");
  return true;
}

static void
example_deinit(mod_t mod)
{
  (void)mod;
  MOD_LOG_INFO("Example mod stopped");
}

MOD_ENTRY()
{
  static const mod_api_t api = {
    .struct_size = sizeof(mod_api_t),
    .abi_version = MOD_ABI_VERSION,
    .init        = example_init,
    .deinit      = example_deinit,
  };

  return &api;
}
```

`mod_entry` only returns the callback table. Do not perform real initialization there. Overdub calls `init` later on the game thread after its engine hooks are ready.

`init` is required. Other callbacks are optional:
```c
struct mod_api_s {
  uint32_t                  struct_size;
  mod_version_t             abi_version;
  mod_init_fn_t             init;
  mod_deinit_fn_t           deinit;
  mod_tick_fn_t             tick;
  mod_input_fn_t            input;
  mod_pe_pre_fn_t           pe_pre;
  mod_pe_post_fn_t          pe_post;
  mod_func_invoke_pre_fn_t  func_invoke_pre;
  mod_func_invoke_post_fn_t func_invoke_post;
  mod_draw_panel_fn_t       draw_panel;
  mod_draw_config_fn_t      draw_config;
};
```

Always set `struct_size` and `abi_version`. Major and minor ABI versions must match the loader. Patch differences are ignored.

## 5. Lifecycle and Cleanup

The useful distinction is between stopping and reloading:
```
Start:   init
Stop:    deinit, managed resources removed, DLL stays loaded
Restart: Stop + Start
Reload:  Stop + unload DLL + load new DLL + Start
```

Static DLL variables survive Stop and Start because the DLL remains loaded.
```c
static int g_counter;
```

Reset per-run state in `init`, and clear pointers in `deinit`.

After `deinit`, Overdub removes SDK-managed hooks, UObject listeners, commands, and mod arenas.

The mod must clean up anything Overdub does not own, such as threads, OS handles, external allocations, hooks, patches , etc.

A DLL cannot be unloaded while another thread or callback can still execute its code. During `deinit`, stop new work, disable unmanaged callbacks, signal and join every worker thread, restore patches, and only then return.

## 6. Threads and Hot Paths

`init`, `deinit`, `tick`, UI callbacks, and most input and reflected-call callbacks normally run on the game thread.

Most UObject and gameplay work belongs on the game thread.

A custom hook runs on whichever thread called the hooked function. Before installing one, determine whether it can run concurrently, recurse, execute during shutdown, etc.

Keep per-frame callbacks, listeners, hooks, etc. fast. Move file IO, parsing, compression, and similar slow work to worker threads, but copy any borrowed game data before sending it to a worker.

Do not log every frame or every reflected call in normal use.

## 7. Memory

### Permanent arena

```c
mod_arena_t arena = mod_get_perm();
```

Use it for state that lives for one active run of the mod. It is destroyed after `deinit`.

```c
typedef struct {
  int counter;
} state_t;

static state_t *g_state;

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  g_state = MOD_ARENA_PUSH_ZERO(mod_get_perm(), state_t);
  return g_state != NULL;
}

static void
example_deinit(mod_t mod)
{
  (void)mod;
  g_state = NULL;
}
```

### Dynamic arena

```c
mod_arena_t arena = mod_arena_create(64 * MB, 64 * KB);
```

Use a dynamic arena when a subsystem needs its own reset or destroy point.

### Scratch arena

```c
tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);

/* temporary allocations */

mod_scratch_end(tmp);
```

Scratch pointers become invalid at `mod_scratch_end`. Never store them.

Treat `mod_t`, `mod_cfg_t`, and `mod_arena_t` as opaque handles. Do not cast them to pointers or depend on their representation.

## 8. Unreal Runtime Basics

The few relationships that matter most are:

```
class       what type an object is
super class what its class inherits from
outer       which UObject contains it
world/level where a runtime actor exists
```

The outer chain is not an inheritance chain.

Every actor is a UObject, but many UObjects are not actors. Classes, functions, assets, etc. are also UObjects.

A UObject contains a class, name, outer, flags, and an index in the global UObject array. Overdub exposes the current target layouts through `mod_unreal.h`.

### Object lifetime

Treat every `uobject_t *` as a borrowed pointer. Objects may disappear during travel, level streaming, gameplay destruction, or garbage collection. Slots in the global UObject array can be reused.

Validate cached pointers before use:

```c
if (!obj || !unreal_uobject_is_valid(obj)) {
  obj = NULL;
}
```

Validation only proves that the object is valid at that moment. Clear tracked pointers when objects are deleted and during `deinit`.

### Names and full names

Unreal uses `FName` IDs rather than ordinary string pointers for many names. Use `FNAME_FIND` when you only need an existing name. Avoid adding names to the global pool without a reason.

Short object names can repeat. Prefer full names when identifying a known class or function.

```
Class /Script/Engine.Actor
Function /Script/Engine.Actor.K2_DestroyActor
```

## 9. Finding UObjects

Include the Unreal helpers and initialize their common cache:

```c
#include "mod_unreal.h"

unreal_cache_objects();
```

Find a class by full name:

```c
uclass_t *actor_class =
  unreal_uobject_find_class_by_full_name(STR_LIT("Class /Script/Engine.Actor"));
```

Find a live instance:

```c
uobject_t *actor = unreal_uobject_find_first_of(actor_class);
```

The first instance is often not the one you want. Filter by full name, world, outer chain, etc.

Check inheritance with:

```c
if (actor && unreal_uobject_is_a(actor, actor_class)) {
  /* actor is Actor or derives from Actor */
}
```

Use the built-in UObject Search tool to inspect names, properties, functions, etc. before hard-coding a lookup.

## 10. UObject Listeners

Listeners are useful for tracking objects that appear and disappear at runtime.

```c
static uobject_t *g_tracked;

static void
on_created(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  /* Match only the objects the mod needs. Keep this callback fast. */
}

static void
on_deleted(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  if (obj == g_tracked) {
    g_tracked = NULL;
  }
}
```

Register them during `init`:

```c
if (!mod_register_uobject_listener(UOBJECT_LISTENER_KIND_CREATE, on_created, NULL)) {
  return false;
}

if (!mod_register_uobject_listener(UOBJECT_LISTENER_KIND_DELETE, on_deleted, NULL)) {
  return false;
}
```

Managed listeners are removed after `deinit`.

## 11. Properties and Reflection

An `FProperty` describes the type and offset of a value. It does not normally contain the value itself.

```c
fprop_t *prop = unreal_ustruct_find_prop((ustruct_t *)obj->cls, STR_LIT("Health"));
void *ptr = unreal_uprop_ptr(prop, obj);
```

Before reading or writing, verify the property exists and has the expected type and size.

Do not treat all properties as integers or raw byte blocks. For example, bool properties may occupy one bit, strings and containers may require construction and destruction, and object properties follow Unreal lifetime rules.

Start with plain numeric values, names, and object pointers. Use the SDK helpers for boolean properties instead of overwriting the containing byte.

## 12. Calling UFunctions

`ProcessEvent` calls a reflected function on a UObject. Parameters are stored in one buffer whose size and member offsets come from reflection.

Do not infer offsets from a C declaration.

```c
/* NOTE: error handling is omitted for briefty */
static bool
call_set_count(uobject_t *obj, int32_t value)
{
  if (!obj || !unreal_uobject_is_valid(obj)) {
    return;
  }

  ufunc_t *func       = unreal_ustruct_find_func((ustruct_t *)obj->cls, STR_LIT("SetCount"));
  fprop_t *count_prop = unreal_ustruct_find_prop((ustruct_t *)func, STR_LIT("Count"));

  tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);
  {
    uint8_t *params = MOD_ARENA_PUSH_ARRAY_ZERO(tmp.arena, uint8_t, func->params_size);
    int32_t *count  = (int32_t *)unreal_uprop_ptr(count_prop, params);

    *count = value;

    unreal_process_event(obj, func, params);
  }
  mod_scratch_end(tmp);
  return true;
}
```

This example is only valid when `Count` is really a plain `int32_t`. Complex inputs, outputs, and return values need their proper Unreal initialization and cleanup.

## 13. ProcessEvent and UFunction::Invoke Callbacks

A mod can observe or replace game-thread reflected calls:

```c
bool pe_pre(mod_t mod, uobject_t *obj, ufunc_t *func, void *params);
void pe_post(mod_t mod, uobject_t *obj, ufunc_t *func, void *params, bool consumed);
```

Returning `true` from `pe_pre` consumes the call and skips the original `ProcessEvent`. Use this carefully. The original may produce return values, update state, run Blueprint logic, etc.

All active pre-callbacks run, and Overdub combines their results. Two mods can conflict when both replace the same call.

`ProcessEvent` can recurse when a callback triggers another reflected call. Use a recursion guard when needed.

Never keep the `params` pointer after the callback.

`UFunction::Invoke` callbacks expose an `FFrame` and result pointer. They are lower level and more fragile than `ProcessEvent`. Never save those pointers. Consume an invoke call only when you fully understand the execution path, because skipping it commonly breaks Blueprint execution.

## 14. Configuration

Get handles during `init`:

```c
static mod_cfg_t g_enabled_cfg;

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  g_enabled_cfg = mod_get_cfg_by_id(STR_LIT("feature_enabled"));
  return g_enabled_cfg != MOD_CFG_INVALID;
}
```

Read the value:

```c
bool enabled = mod_cfg_get_bool(g_enabled_cfg);
```

There is no separate option-changed callback. Compare the current value with the previous value in `tick` when a feature needs transition logic.

The SDK provides typed getters and setters for bool, int, float, enum, string, keybind, and color options. Do not use a handle with the wrong type.

## 15. Input and Keybinds

Direct input callback:

```c
static bool
example_input(mod_t mod, input_event_t *event)
{
  (void)mod;

  if (event->kind == INPUT_EVENT_KEY_DOWN &&
      event->key == INPUT_KEY_F8 &&
      !event->is_repeat) {
    MOD_LOG_INFO("F8 pressed");
    return true;
  }

  return false;
}
```

Return `true` only when the mod intentionally wants to block later mod callbacks and the game input handler.

For a configured keybind, polling from `tick` is often simpler:

```c
keybind_t bind = mod_cfg_get_keybind(g_action_key_cfg);
if (mod_keybind_is_pressed(bind)) {
  /* run once */
}
```

Use `is_down` for held state, `is_pressed` for the up-to-down edge, and `is_released` for the down-to-up edge.

## 16. Console Commands

Register commands during `init`:

```c
static void
example_command(mod_t mod, str_t name, str_t args, void *user)
{
  (void)mod;
  (void)name;
  (void)user;

  MOD_LOG_INFO("args: " STR_FMT, STR_ARG(args));
}

static bool
register_commands(void)
{
  return mod_register_cmd(STR_LIT("example.run"), STR_LIT("Runs the example command"), example_command, NULL);
}
```

Prefix command names to avoid collisions with other mods.

## 17. Custom UI

Include the matching Overdub Nuklear wrapper:

```c
#include "mod_nuklear.h"
```

```c
static void
example_draw_panel(mod_t mod, struct nk_context *ctx)
{
  (void)mod;

  nk_layout_row_dynamic(ctx, 24.0f, 1);
  nk_label(ctx, "Example panel", NK_TEXT_LEFT);

  if (nk_button_label(ctx, "Run")) {
    /* action */
  }
}
```

Overdub uses a modified Nuklear version. Compile against the SDK shipped with the target Overdub version.

Nuklear is immediate mode. Rebuild controls every frame, keep persistent UI state in the mod, and never store the context pointer. Do not block or issue D3D12 rendering directly from mod UI callbacks.

## 18. Signatures and Native Hooks

Use a native hook only when reflection or a higher-level callback is not enough.

An RVA is an offset from the game module base. It is exact for one known executable but normally changes after an update.

A signature is a byte pattern with wildcard bytes:

```
48 89 5C 24 ?? 57 48 83 EC ??
```

A useful signature should match one intended location. A signature surviving an update does not prove that parameters, structure layouts, vtable indexes, or behavior are unchanged.

Example hook shape:

```c
typedef void (UNREAL_CALL *target_fn_t)(void *self);

static target_fn_t g_target_real;

static void UNREAL_CALL
target_hook(void *self)
{
  /* before */
  g_target_real(self);
  /* after */
}
```

Resolve and install it:

```c
static bool
install_target_hook(void)
{
  void *target = NULL;

  sigscan_entry_t entry = {
    .name    = "target",
    .pattern = "48 89 5C 24 ?? 57 48 83 EC ??",
    .pp      = &target,
    .op_off  = 0,
    .deref   = 0,
  };

  if (mod_sigscan(&entry) != SIGSCAN_ERR_OK || !target) {
    return false;
  }

  if (!mod_hook_create(target, target_hook, (void **)&g_target_real)) {
    return false;
  }

  return mod_hook_enable(target);
}
```

The pattern above is only an example.

A detour must match the real ABI, parameters, return type, and calling thread. It must also handle recursion and shutdown correctly. Call the original when the caller depends on its side effects or return value.

Hooks created through the SDK are removed after `deinit`. Manual hooks and patches are the mod's responsibility.

## 19. Assets and Blueprint Actors

Unreal package paths are not Windows paths.

```
/Game/Mods/Example/BP_ExampleMod
```

Mounting a package makes its content available. It does not automatically execute a Blueprint or spawn an actor.

An asset replacement uses the same package path as the original asset. The package with the higher mount priority normally wins. Runtime mod order controls asset priority, so asset conflicts require a restart after reordering.

A cooked Blueprint usually produces a generated class ending in `_C`:

```
Asset: /Game/Mods/Example/BP_ExampleMod
Class: /Game/Mods/Example/BP_ExampleMod.BP_ExampleMod_C
```

Overdub can load the generated class and spawn an actor using a context declared in `mod.ini`.

Blueprint actors may be destroyed during travel or level unloading. Persistent features need to handle `EndPlay`, missing worlds, and respawning when an appropriate context becomes available again.

Common asset problems include the wrong cooked target, an incorrect package or generated-class path, missing dependencies, etc.

## 20. Debugging Tools

### Logging

```c
MOD_LOG_DEBUG("value: %d", value);
MOD_LOG_INFO("mod started");
MOD_LOG_WARN("object not found");
MOD_LOG_ERROR("failed to install hook");
```

Log initialization stages and failures with enough context to reproduce them. Avoid hot-path spam.

### UObject Search

Use UObject Search instead of guessing names and layouts. It shows properties, functions, inheritance, etc.

Selecting a result opens a reusable preview tab. Click the tab to pin it. The details panel shortcuts are:

| Shortcut   | Action              |
|------------|---------------------|
| `Tab`      | Select next tab     |
| `Ctrl+Tab` | Select previous tab |
| `Ctrl+W`   | Close selected tab  |

### UFunction Tracer

Start with a narrow filter. Capturing every reflected call can fill the call limit and slow the game.

Rules use this form:
```
+target:text
-target:text
```

`+` includes and `-` excludes. An omitted target means `func`.

Recommended targets:
```
self
self_outer
class
class_outer
class_inherit
func
func_outer
```

Include rules use OR. Any matching exclusion rejects the call. With only exclusions, all calls start included.

Example:
```
+class_inherit:Actor
-func:Tick
-self:Default__
```

Matching is substring-based by default. `Ignore case` and `Exact match` apply to the whole filter.

`Capture nested calls` records calls below an already captured call without filtering each nested call. It is useful for call trees but may produce many results.

The tracer does not currently save parameter or return values. Captured UObject pointers may become invalid later.

### Native debugger

Hi-Fi RUSH is protected by VMProtect, so ordinary debugger attachment is blocked. For game builds where a working bypass is available, build the mod with PDB files and debug the first invalid write rather than only the later crash.

When reporting a crash, keep:
- `overdub.log`
- stack trace
- game and store version
- Overdub version
- mod version
- installed mod list
- exact reproduction steps
- PDB files for native mods

## 21. Reload-Safe Checklist
### `init`
- reset per-run globals
- call `mod_sdk_init`
- acquire required config handles
- allocate state
- resolve required classes and functions
- register managed hooks, listeners, and commands
- start threads only after all shared state is ready
- return `false` on a required initialization failure

### Runtime
- validate cached UObject pointers
- handle world and level changes
- keep callbacks fast
- avoid hot-path logging
- guard recursive hooks
- copy borrowed data before passing it to workers
- never retain parameter buffers, `FFrame`, result pointers, or scratch pointers

### `deinit`
- mark the mod as shutting down
- stop producing new jobs
- disable unmanaged callbacks
- signal and join threads
- restore manual patches
- unregister direct engine callbacks and delegates
- release handles, COM objects, and external memory
- clear UObject and arena pointers
- return only after no code can call into the DLL

## 22. Source Map
When this guide and the implementation disagree, check the code.

| Area                             | Source                                   |
|----------------------------------|------------------------------------------|
| Loader entry and main callbacks  | `src/main.c`                             |
| Signatures and core engine hooks | `src/signatures.c`                       |
| Manifest parser and lifecycle    | `src/mod_manager.c`                      |
| Host API                         | `src/mod_host.c`, `include/mod_host.h`   |
| Public mod ABI                   | `mod/mod_api.h`                          |
| SDK wrappers                     | `mod/mod_sdk.h`, `mod/mod_sdk.c`         |
| Unreal helpers                   | `mod/mod_unreal.h`, `mod/mod_unreal.c`   |
| Nuklear helpers                  | `mod/mod_nuklear.h`, `mod/mod_nuklear.c` |
| D3D12 overlay                    | `src/nk_d3d12.c`                         |
| Built-in tool examples           | `src/builtin/`                           |
