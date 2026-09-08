# Overdub

[![CI](https://github.com/akmubi/overdub/actions/workflows/ci.yml/badge.svg)](https://github.com/akmubi/overdub/actions/workflows/ci.yml)

Overdub is a mod loader and in-game mod manager for Hi-Fi RUSH. It can load native DLLs, Lua scripts, cooked Unreal assets, and Blueprint actors from one mod package.

The loader includes a mod manager, a console, UObject Search, and UFunction Tracer. Native and Lua mods can use Unreal reflection, receive game callbacks, and draw controls in the shared UI.

Overdub supports the Steam, Epic Games, and Xbox/Game Pass releases. It also runs through Proton on Linux and Steam Deck.

## Install

Download the release for your game version. Copy `overdub.dll` and the included bootstrap DLL into the game binary directory.

For Steam and Epic Games:
```
Hibiki/Binaries/Win64/
|-- overdub.dll
+-- XAPOFX1_5.dll
```

For Xbox and Game Pass:
```
Hibiki/Binaries/WinGDK/
|-- overdub.dll
+-- dsound.dll
```

Do not keep UE4SS or another proxy DLL loader in the same directory. Start the game once and Overdub will create its `mods` directory and configuration files.

See [Installation](docs/INSTALLATION.md) for the complete setup and update instructions.

## Use

The default shortcuts are:

| Key      | Window           |
| -------- | ---------------- |
| Backtick | Mod manager      |
| F1       | Console          |
| F2       | UObject Search   |
| F3       | UFunction Tracer |

Each installed mod has its own directory under `mods` and must contain `mod.ini`.

```
mods/
+-- example-mod/
    |-- mod.ini
    |-- example.dll
    |-- scripts/
    |   +-- main.lua
    +-- assets/
```

See [Using Overdub](docs/USING_OVERDUB.md) for mod installation, controls, configuration, and built-in tools. See [Troubleshooting](docs/TROUBLESHOOTING.md) when Overdub or a mod does not start.

## Make mods

Overdub provides a C SDK and a native Lua 5.4 runtime. It also has a separate compatibility runtime for existing UE4SS Lua mods.

Start with the [Mod Development](docs/MOD_DEVELOPMENT.md) page. It links to the package format, build guide, native SDK, Lua API, architecture, and caveats.

## Build Overdub

Open `overdub.sln` in Visual Studio 2022 and build `Release|x64`, or cross-compile the Windows DLL with MinGW-w64:

```sh
make release
```

The output is `build/overdub.dll`. The Debug and Release configurations are described in [Building Native Mods](docs/mod-development/building-native-mods.md).

## License

See [LICENSE](LICENSE).

Overdub is an unofficial project and is not affiliated with the developers or publishers of Hi-Fi RUSH.
