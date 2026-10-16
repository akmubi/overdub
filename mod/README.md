# Overdub Native Mod Template

This directory is a small native mod project. In a release mod template, the `overdub` folder holds the Overdub source the mod builds against, and the build makes `overdub/build/overdub.lib` from it. Use the template of the same Overdub release that the player has installed.

Build with Visual Studio 2022:

```batch
msbuild mod.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:ModName=example
```

Or use the Makefile from an MSYS2 MinGW64 shell or Linux with MinGW-w64:

```sh
make CONFIG=release MOD_NAME=example
```

The DLL is written below `build`. Copy it into a mod directory with this manifest:

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

The source template is in `main.c`. The full guide starts at `overdub/docs/MOD_DEVELOPMENT.md`.
