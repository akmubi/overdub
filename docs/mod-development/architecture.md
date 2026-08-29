# Architecture

This page explains the boundaries a mod author needs to understand. It is not a source-level tour of every Overdub subsystem.

## Loader and mod package

The loader discovers package manifests at startup and builds one ordered mod list. A package can combine DLL code, Lua code, assets, and Blueprint actor declarations. The mod manager presents those parts as one mod and coordinates their start and stop operations.

Assets are mounted at startup because Unreal package unloading is not a safe general reload mechanism. DLL and Lua runtimes have explicit lifecycles and can be restarted while the game is running.

## Native ABI

A native DLL exports an ABI version and a callback table. The table keeps the entry ABI small and allows optional callbacks to be added through `struct_size` checks.

Overdub passes an opaque mod handle to each callback. Registrations made through this handle are tagged with the active mod run. This lets the loader remove commands, hooks, listeners, arenas, and custom function implementations without exposing internal generation IDs to mod code.

The DLL remains loaded during Stop and Start. Reload is the separate operation that unloads it. This distinction lets users temporarily disable a feature without forcing Windows to reload code, while still giving developers a way to replace a DLL.

## Lua runtimes

Native Overdub Lua and UE4SS-compatible Lua are different runtimes. They share the Lua VM wrapper and the low-level Unreal property codec, but they do not share lifecycle rules or public globals.

Each native Overdub Lua mod owns one Lua state. The game thread owns that state and runs its lifecycle, Unreal hooks, task resumes, future completion callbacks, and UI callbacks. Modules are imported with `require("overdub")` and related package names. The entry chunk returns its lifecycle table.

Each UE4SS-compatible mod has its own state and legacy API. Its scheduling and callback behavior follows the compatibility layer. No `overdub` packages are installed in that state.

Keeping the runtimes separate prevents a legacy global or worker-thread assumption from silently changing native Overdub behavior.

## Cooperative tasks and workers

A cooperative task is a Lua coroutine on the game thread. It can access Unreal objects because it never leaves that thread. It must yield so other frame work can run.

A worker job runs a named Lua module function in a separate Lua state. Only copied serializable values cross the boundary. Lua closures, UObjects, UI contexts, coroutines, and userdata remain in their owning state.

This split exists because moving a game-thread Lua closure to a worker would also move its captured state and could expose Unreal pointers on the wrong thread. Named functions and copied values make the boundary visible and testable.

## Unreal property codec

The shared property codec owns the rules for constructing, copying, reading, writing, and destroying reflected values. UObject Search, UFunction calls, the tracer, the UE4SS layer, the native Lua layer, and custom reflection all use the same rules.

This avoids having one subsystem copy an `FString` correctly while another treats it as plain bytes. It also keeps container and object-reference validation consistent.

## UI

Native DLL callbacks receive Overdub's Nuklear context. Native Lua receives a restricted userdata wrapper around the same context. Both draw through the normal mod tick and panel routes.

The context is borrowed for one draw callback. Lua does not see its raw pointer. Persistent UI state belongs to the mod, while widget declarations are rebuilt each frame.

## Custom reflected classes

Custom classes are process-lifetime Unreal types. Their layouts cannot be unregistered safely after objects, references, or cached reflection data may exist. Overdub therefore leaves the class registered when a mod stops and disables only the mod-owned function implementations.

Definitions are scoped by mod ID to prevent name collisions. A later start of the same mod may reuse an identical class definition. A conflicting definition fails instead of changing a live layout.

## Managed hook chain

Hooks created through the native mod API join Overdub's chain for that target. The most recently enabled hook runs first, and each saved original pointer remains a stable call to the next entry. Removing one mod's hook rewires the chain without invalidating another mod's original pointer.

Direct MinHook calls do not join this chain and can still conflict with it.
