# Installation

Overdub releases are tied to a supported game build. Download the archive for the store and game version you use.

## Windows

Close the game before changing loader files.

For Steam and Epic Games, open the game directory and copy these files into `Hibiki/Binaries/Win64`:
```
overdub.dll
XAPOFX1_5.dll
```

For Xbox and Game Pass, copy these files into `Hibiki/Binaries/WinGDK`:
```
overdub.dll
dsound.dll
```

The two DLLs must be in the same directory as the game executable. Do not rename them.

Remove UE4SS and other proxy DLL loaders from that directory. Two loaders trying to use the same proxy entry point will conflict.

Start the game. A working installation creates these items beside the game content:
```
mods/
overdub-config.ini
overdub.log
```

## Linux and Steam Deck

Use the same Windows release files. Copy them into the Windows game directory inside the Proton prefix. Overdub does not require a Wine DLL override or a launch option.

## Update

Close the game, replace `overdub.dll`, and replace the bootstrap DLL when the release includes a new one. Keep `mods`, `overdub-config.ini`, and mod `config.ini` files unless the release notes say otherwise.

Native mods must use an ABI version compatible with the installed Overdub build. When a native mod stops loading after an update, update that mod as well.

## Uninstall

Close the game and remove `overdub.dll` together with `XAPOFX1_5.dll` or `dsound.dll`.

The `mods` directory and INI files contain installed mods and saved settings. Remove them only if you also want to remove that data.
