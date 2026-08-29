# Building Native Mods

The release SDK template is self-contained. It has the public headers, Nuklear headers, `build/overdub.lib`, a sample source file, and project files for Visual Studio and MinGW-w64. Use the template from the same Overdub release that the player has installed.

## Visual Studio

The project uses Visual Studio 2022, x64, C11 or C++20, and the static MSVC runtime.

Build a debug DLL:

```batch
msbuild mod.vcxproj /t:Build /m /p:Configuration=Debug /p:Platform=x64 /p:ModName=example
```

Build a release DLL:

```batch
msbuild mod.vcxproj /t:Build /m /p:Configuration=Release /p:Platform=x64 /p:ModName=example
```

The DLL is written under `build/Debug` or `build/Release`.

## MinGW-w64

Run the Makefile from an MSYS2 MinGW64 shell on Windows, or install the MinGW-w64 cross compiler on Linux.

```sh
make CONFIG=debug MOD_NAME=example
make CONFIG=release MOD_NAME=example
```

Set `MOD_SRC` when the mod has another source filename or several source files:

```sh
make CONFIG=release MOD_NAME=example MOD_SRC="main.c feature.c"
```

## Use a source checkout

The in-repository template finds the parent Overdub checkout automatically. A separate project can point to one explicitly:

```batch
msbuild mod.vcxproj /p:OverdubDir=C:\src\overdub /p:Configuration=Release /p:Platform=x64 /p:ModName=example
```

```sh
make OVERDUB_DIR=/src/overdub CONFIG=release MOD_NAME=example
```

When `build/overdub.lib` is missing from a source checkout, the template builds Overdub first. A packaged release template already contains the import library and does not need the Overdub source.

## Package the result

Copy the DLL into a mod directory and add `mod.ini`:
```
example-mod/
|-- mod.ini
+-- example.dll
```

```ini
[info]
id      = author.example
name    = Example Mod
author  = Author Name
version = 1.0.0
kind    = mod

[code]
path = example.dll
```

Copy the directory into `mods` and restart the game. During development, Overdub can reload the DLL from the mod manager after it has been discovered.

## Build Overdub itself

Open `overdub.sln` and build `Debug|x64` or `Release|x64`. The equivalent MinGW-w64 commands are:

```sh
make debug
make release
```
