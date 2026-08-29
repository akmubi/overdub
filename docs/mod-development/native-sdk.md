# Native SDK

Include `mod.h` for the native mod ABI. It also includes the common input, logging, signature scanning, Unreal, reflection, string, and version declarations.

The mod links to `overdub.lib`. The implementation remains in `overdub.dll`, so the SDK headers and import library must come from a compatible release.

## Entry points

Every native mod exports its ABI version and a callback table. `init` is required. The other callbacks may be left null.

```c
#include "mod.h"

static bool
example_init(mod_handle_t mod)
{
  MOD_LOG_INFO(mod, "Example mod started");
  return true;
}

static void
example_deinit(mod_handle_t mod)
{
  MOD_LOG_INFO(mod, "Example mod stopped");
}

MOD_ABI_VERSION_ENTRY()
{
  return MOD_ABI_VERSION;
}

MOD_ENTRY()
{
  static const mod_api_t api = {
    .struct_size = sizeof(mod_api_t),
    .init        = example_init,
    .deinit      = example_deinit,
  };

  return &api;
}
```

`mod_entry` should only return the table. Overdub calls `init` later, after its engine hooks and manager state are ready. Returning false from `init` aborts that start.

The callback table can also receive `tick`, `input`, `pe_pre`, `pe_post`, `func_invoke_pre`, `func_invoke_post`, `draw_panel`, and `draw_config`. See [`mod_api_t`](../../include/mod.h) for their exact signatures.

## Lifecycle

Stop calls `deinit` and removes resources registered through the mod handle, but it leaves the DLL loaded. Restart performs Stop followed by Start. Reload stops the mod, unloads the DLL, loads it again, and starts it.

Static variables therefore survive Stop and Start. Reset per-run state in `init` and clear borrowed pointers in `deinit`.

```c
static mod_handle_t g_mod;
static unsigned int g_ticks;

static bool
example_init(mod_handle_t mod)
{
  g_mod   = mod;
  g_ticks = 0;
  return true;
}

static void
example_deinit(mod_handle_t mod)
{
  (void)mod;
  g_mod = MOD_HANDLE_INVALID;
}
```

Overdub owns commands, managed hooks, UObject listeners, custom class implementations, and arenas registered with the mod handle. It removes or disables them after `deinit`. The mod still owns raw OS handles, its own threads, direct patches, and anything registered outside the SDK.

## State and memory

`mod_get_perm` returns an arena that lives for the current active run. Use it for ordinary mod state instead of scattering allocations across `malloc` and `free`.

```c
typedef struct example_state_s example_state_t;
struct example_state_s {
  unsigned int updates;
  uobject_t   *target;
};

static example_state_t *g_state;

static bool
example_init(mod_handle_t mod)
{
  g_state = ARENA_PUSH_ZERO(mod_get_perm(mod), example_state_t);
  return g_state != NULL;
}

static void
example_deinit(mod_handle_t mod)
{
  (void)mod;
  g_state = NULL;
}
```

`mod_arena_create` creates another manager-owned arena when one subsystem needs a separate lifetime. Destroy it early with `mod_arena_destroy`, or let Overdub remove it after `deinit`.

Use `scratch_begin` and `scratch_end` for temporary work. Every path after `scratch_begin` must reach `scratch_end`, and no pointer into the scratch arena may survive it.

```c
tmp_arena_t tmp = scratch_begin(NULL);
str_t       text = str_push_fmt(tmp.arena, "frame %llu", mod_get_frame_counter());

MOD_LOG_INFO(mod, STR_FMT, STR_ARG(text));
scratch_end(tmp);
```

## Tick and input

Tick runs once per game frame. Keep it short and avoid file access, long searches, and repeated logging.

```c
static void
example_tick(mod_handle_t mod, float delta)
{
  (void)delta;

  if ((mod_get_frame_counter() % 300) == 0) {
    MOD_LOG_DEBUG(mod, "still active");
  }
}
```

Input receives one event. Return true only when the event should not reach later mods or the game.

```c
static bool
example_input(mod_handle_t mod, input_event_t *event)
{
  bool consume = false;
  if (event->kind == INPUT_EVENT_KEY_DOWN && event->key == INPUT_KEY_F8 && !event->is_repeat) {
    MOD_LOG_INFO(mod, "F8 pressed");
    consume = true;
  }

  return consume;
}
```

Manifest keybind options are often easier to poll from tick. Get the option once during `init`, then use `mod_cfg_get_keybind` and the input helpers.

## Configuration

`mod_get_cfg_by_id` finds an option declared in `mod.ini`. The handle remains owned by Overdub.

```c
static mod_cfg_handle_t g_enabled_cfg;

static bool
example_init(mod_handle_t mod)
{
  g_enabled_cfg = mod_get_cfg_by_id(mod, STR_LIT("enabled"));
  return g_enabled_cfg != MOD_CFG_HANDLE_INVALID;
}

static void
example_tick(mod_handle_t mod, float delta)
{
  (void)mod;
  (void)delta;

  if (mod_cfg_get_bool(g_enabled_cfg)) {
    run_feature();
  }
}
```

Use the getter or setter that matches the declared option type. String getters can report their length, copy into a buffer, or copy into an arena.

## Console commands

Register commands during `init`. Prefix the command name with the mod ID or another unique name.

```c
static void
example_command(mod_handle_t mod, str_t name, str_t args, void *user)
{
  (void)name;
  (void)user;
  MOD_LOG_INFO(mod, "command args: " STR_FMT, STR_ARG(args));
}

static bool
example_init(mod_handle_t mod)
{
  return mod_register_cmd(mod, STR_LIT("example.run"), STR_LIT("Runs the example action"), example_command, NULL);
}
```

The registration belongs to the current mod run and is removed automatically at shutdown.

## UI

`draw_panel` draws the mod's main panel. `draw_config` draws the Config section and replaces the automatic controls for that section. Both receive Overdub's public Nuklear context.

```c
#include "vendor_nuklear.h"

static void
example_draw_panel(mod_handle_t mod, struct nk_context *ctx)
{
  (void)mod;

  nk_layout_row_dynamic(ctx, 24.0f, 1);
  nk_label(ctx, "Example panel", NK_TEXT_LEFT);

  if (nk_button_label(ctx, "Run")) {
    run_feature();
  }
}
```

Nuklear is immediate mode. Store persistent values in mod state and rebuild the controls on every draw. Do not keep the context pointer after the callback.

## Signature scans and hooks

Use a native hook only when Unreal reflection or a loader callback cannot provide the event you need. A signature identifies code in one supported executable build; it does not prove that the function ABI or surrounding data layout stayed compatible.

```c
typedef void (UNREAL_CALL *target_fn_t)(void *self);

static target_fn_t g_target_real;

static void UNREAL_CALL
target_hook(void *self)
{
  g_target_real(self);
}

static bool
install_hook(mod_handle_t mod)
{
  void *target = NULL;
  sigscan_entry_t entry = {
    .name    = "target",
    .pattern = "48 89 5C 24 ?? 57 48 83 EC ??",
    .pp      = &target,
    .kind    = SIG_DIRECT,
  };

  bool installed = mod_sigscan(&entry) == SIG_ERR_OK && target;
  if (installed) {
    installed = mod_hook_create(mod, target, target_hook, (void **)&g_target_real);
  }

  if (installed) {
    installed = mod_hook_enable(mod, target);
  }

  return installed;
}
```

Managed hooks on the same address form a chain. The most recently enabled hook runs first. Hooks installed directly through another MinHook instance do not join that chain and may conflict.

A detour runs on whichever thread called its target. Match the exact calling convention, parameters, return value, recursion behavior, and lifetime. Overdub removes managed hooks after `deinit`, but the mod must remove direct hooks and restore direct patches itself.
