# Overdub Mod Template

This directory contains a minimal native Overdub mod and its matching SDK.
`include/`, `vendor/`, and `build/overdub.lib` are contained in this project,
so no Overdub source checkout or `OverdubDir` setting is required.

Build with Visual Studio 2022:

```batch
msbuild mod.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:ModName=example
```

Or build from an MSYS2 MinGW64 shell:

```sh
make CONFIG=release MOD_NAME=example
```

The DLL is written below `build/`. Copy the DLL into the native mod package
described by `docs/MOD_DEVELOPMENT.md`.

Keep the headers and `overdub.lib` from the same Overdub release. The mod DLL
imports its SDK functions from `overdub.dll`; it does not contain a static SDK.
