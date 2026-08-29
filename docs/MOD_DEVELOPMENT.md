# Mod Development

An Overdub mod can contain a native DLL, a Lua entry script, Unreal assets, Blueprint actors, or any combination of them. Every mod is a directory with a `mod.ini` manifest.

If you want to make a Lua mod, begin with [Native Overdub Lua](lua/overdub-runtime.md). Existing UE4SS scripts use the separate [UE4SS Compatibility Runtime](lua/ue4ss-runtime.md).

If you want to make a native mod, download the SDK template from the same Overdub release used by the player. Read [Building Native Mods](mod-development/building-native-mods.md), then [Native SDK](mod-development/native-sdk.md).

The rest of the guide is split by subject:

| Document                                                        | Subject                                                           |
| --------------------------------------------------------------- | ----------------------------------------------------------------- |
| [Package Format](mod-development/package-format.md)             | `mod.ini`, assets, Blueprints, and saved options                  |
| [Building Native Mods](mod-development/building-native-mods.md) | Visual Studio, MinGW-w64, and package output                      |
| [Native SDK](mod-development/native-sdk.md)                     | DLL entry points, callbacks, memory, commands, input, and hooks   |
| [Unreal API](mod-development/unreal.md)                         | UObjects, properties, UFunctions, listeners, and custom classes   |
| [Architecture](mod-development/architecture.md)                 | Loader ownership, Lua runtimes, callbacks, and reload behavior    |
| [Caveats](mod-development/caveats.md)                           | Object lifetime, threads, asset lifetime, and unsafe engine calls |

The public C declarations are in [`include`](../include). The headers are the final reference for function signatures and structure fields. The guides explain how the API is meant to be used.
