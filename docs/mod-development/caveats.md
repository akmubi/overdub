# Caveats

Overdub can make engine access easier, but it cannot make every engine operation safe. Keep these boundaries in mind while developing and testing a mod.

## Game versions

Signatures, offsets, structure layouts, vtable slots, and native function ABIs can change after a game update. A signature that still matches does not prove that the target function has the same parameters or behavior.

Use a release built for the exact game version. Recheck every native hook after an update.

## UObject lifetime

UObject pointers are borrowed. Objects may be deleted during gameplay, travel, streaming, or garbage collection, and object-array slots can be reused.

Validate a cached pointer before use. For objects that appear later, combine an initial targeted search with create and delete listeners. Do not repeatedly scan the complete object array from tick.

Root only objects that the mod owns and deliberately keeps alive. Rooting a world-owned actor can interfere with normal destruction and travel.

## Callback lifetime

Do not retain `FFrame`, parameter buffers, return pointers, UI contexts, or scratch allocations after their callback ends. They are borrowed storage.

A Lua reflected struct, row, or nested container view can also become invalid when its owning object or row is removed. Check `IsValid` when a value survives across frames.

## Threads

Loader lifecycle, tick, UI, and normal native Lua callbacks run on the game thread. A native detour runs on the thread that called its target, which may be different.

Do not access ordinary UObjects from a worker thread. Copy game data into plain worker-owned memory before starting slow parsing, compression, or network work. Send only copied results back to the game thread.

Lua instruction limits can stop runaway Lua bytecode. They cannot interrupt a blocking C function or operating-system call.

## Reload

Overdub removes resources registered through the mod handle, but it cannot discover everything a DLL created. Before `deinit` returns, a native mod must stop and join its threads, remove direct callbacks, restore patches, and release operating-system or library handles.

No code may execute inside a DLL after Windows unloads it.

Custom reflected classes are not unloaded. Their native or Lua function implementations become inactive when the owning mod stops.

## Assets and Blueprints

Mounted assets remain mounted until process exit. Changing an asset, its enabled state, or mod order requires a game restart.

Blueprint actors can be destroyed during level travel. A persistent Blueprint feature must handle missing spawn contexts and later respawn.

Cooked assets must target the game's Unreal build and include their dependencies. Windows paths are not Unreal object paths.

## Engine calls

Calling a reflected function is synchronous unless the function itself schedules work. The caller must provide a compatible object and valid engine state. Latent, destructive, network, and internal functions may have requirements that reflection metadata does not describe.

Hook callbacks that modify parameters, skip a call, or replace a return value run in the original call path. Keep them small and avoid recursion.

## Diagnostics

Do not log every tick or every call in a busy hook. Use narrow UFunction Tracer filters and stop capture as soon as the relevant action is recorded.

Keep PDB files for native releases. A crash report is most useful when it includes the stack trace, log, exact game version, mod version, and reproduction steps.
