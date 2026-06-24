# Overdub
Overdub is a mod loader and in-game mod manager for Hi-Fi RUSH. It supports native code, Unreal assets, Blueprint actors, etc.

## Features
- Native C mod API with callbacks, hooks, Unreal helpers, etc.
- Start, stop, and reload DLL mods without restarting the game.
- Combine code, assets, etc. in one mod.
- Shared UI for mod settings and keybinds.
- Built-in UObject Search, UFunction Tracer, and console.
- Windows support, including Linux and Steam Deck through Proton.
Supported stores: Steam, Epic Games, and Xbox/Game Pass.

## Installation
Download the release made for your game version. It contains `overdub.dll` and a [Hibiki Bootstrap](https://github.com/akmubi/hibiki-bootstrap) DLL.
Copy both files into the game directory.
Steam and Epic Games:
```
<game>/Hibiki/Binaries/Win64/
|-- overdub.dll
+-- XAPOFX1_5.dll
```
Xbox and Game Pass:
```
<game>/Hibiki/Binaries/WinGDK/
|-- overdub.dll
+-- dsound.dll
```
Linux and Steam Deck use the same files. Copy them into the Windows game directory used by Proton. No DLL override or launch option is needed.
Do not keep **UE4SS** or another **proxy DLL loader** in the same directory.

## First Launch
Start the game. Overdub should create:
```
mods/
overdub-config.ini
overdub.log
```

Default shortcuts:
| Key     | Action                |
|---------|-----------------------|
| `` ` `` | Open the mod manager  |
| `F1`    | Open the console      |
| `F2`    | Open UObject Search   |
| `F3`    | Open UFunction tracer |

If the files above are not created, check the installation path and make sure the bootstrap DLL matches your game version.

## Installing Mods
Extract each mod into its own directory under `mods`. Every mod needs a `mod.ini` manifest.
```
mods/
+-- example-mod/
    |-- mod.ini
    |-- main.dll
    +-- ...
```
Asset mods may also contain `.pak`, `.utoc`, and `.ucas` files.
Restart the game after adding or removing a mod. Overdub discovers mod directories only during startup.

## Using the Mod Manager
The mod list is on the left. Select a mod to see its settings, runtime state, and controls on the right.
The status dot is green when something is active, gray when the mod is inactive, and red when one of its parts failed.
![Selected mod details](docs/images/mod-details.png)

### Enabling Mods
`Enable` and `Disable` control the whole mod. DLL code and tracked Blueprint actors update immediately.
Assets are different. They are mounted during startup and cannot be safely removed while the game is running. Restart after changing an asset mod, its enabled state, or its order.
Disabling a mod may not undo changes it already made to the game.

### Mod Options
Options in the `Config` section change immediately in memory.
- `Save` writes them to `config.ini`.
- `Discard` restores the saved values.
- `Restore defaults` resets them until you save again.
Some mods only apply changed options after their DLL or the game is restarted.

### DLL Controls
The `Code` section separates whether the DLL is loaded from whether its mod code is running.
- `Start` loads and initializes the mod.
- `Stop` shuts it down but keeps the DLL loaded.
- `Restart` stops and starts it.
- `Reload` unloads the DLL, loads it again, and starts it.
Reload is mainly for development. A mod that does not clean up its hooks, callbacks, threads, etc. may crash the game when reloaded.
Reloading DLL code does not rescan manifests, discover new mods, or remount assets.

### Blueprint Mods
The `Blueprints` section shows each declared actor and its current state. You can spawn or despawn it, enable automatic spawning, or assign a spawn keybind.
Spawning may fail until the required world, player, or custom object exists.

### Mod Order
Press `Reorder mods`, select a mod, and move it up or down. Apply the change and restart the game.
Order affects DLL startup, callback processing, and asset priority. Callbacks run from top to bottom. For conflicting assets, the lower mod normally takes priority.
![Reorder Mods](docs/images/reorder-mods.png)

### Settings
Press `Settings` below the mod list to change the mod directory, manager shortcut, console options, etc.
Press `Apply` to save changes to `overdub-config.ini`. Closing the window does not save them. Changing the mod directory also requires a restart.
![Overdub settings](docs/images/settings.png)

## Configuration Files
Overdub stores loader settings in `overdub-config.ini`. Each mod stores its own values in `mods/<mod-directory>/config.ini`.
Edit these files only while the game is closed. Invalid values may be ignored, and the running game may overwrite manual changes.

## Console
Press `F1` to open the console. It shows loader and mod messages and accepts commands registered by mods.
The console contains only the current session. Use `overdub.log` for startup errors or older messages.
![Overdub console](docs/images/console.png)

## Built-in Tools
Built-in tools appear in the normal mod list and can be configured like other mods.

### UObject Search
UObject Search inspects Unreal objects currently loaded in memory. Enter a name, adjust the search settings when needed, and select a result to inspect it.
It can search names, type instances, properties, etc. The details panel shows object information, reflected fields, functions, and Kismet bytecode when available.
Selecting results reuses a preview tab. Click the tab to pin it. Useful shortcuts are `Tab`, `Ctrl+Tab`, and `Ctrl+W`.
The tool is read-only. It cannot edit properties or call functions, and broad searches may be slow.
![UObject Search](docs/images/uobject-search.png)

### UFunction Tracer
UFunction Tracer records Unreal function calls.
1. Add a narrow capture filter.
2. Start capture and perform the action in-game.
3. Stop capture and inspect the calls.
4. Save the trace when needed.
Filters use `+` to include and `-` to exclude:
```
+func:OnBeat
-func:Tick
```
Targets include `func`, `self`, `class`, etc., with outer-chain variants. Include rules use OR, while any matching exclude rule rejects the call.
Start narrow. Busy functions can fill the capture quickly and broad tracing can hurt performance. Parameters and return values are not currently saved.
![UFunction Tracer](docs/images/ufunction-tracer.png)

### Tweaks
The Tweaks tool currently includes tutorial popup control and jukebox state restoration.
Jukebox state is shared across save slots, so restoring it on another slot may restore music that has not been unlocked there.

## Troubleshooting
### Overdub Does Not Load
Check that `overdub.dll` and the correct bootstrap DLL are in the same game directory. Remove UE4SS and other proxy DLL loaders.
If `overdub.log` and `mods` are not created, the bootstrap did not load Overdub.

### The Manager Does Not Open
Press the backtick key rather than the apostrophe key. Also check the keyboard layout, the configured shortcut, and whether the game is still loading.

### A Mod Does Not Appear
Make sure the mod has its own directory, contains `mod.ini`, and uses a unique mod ID. Restart the game and check `overdub.log` for manifest errors.

### A DLL Mod Does Not Start
Open its `Code` section and read the error. Common causes are a wrong DLL path, unsupported API version, or failed initialization.

### An Asset Mod Does Not Work
Open its `Assets` section and check the mount error. Make sure every file named in the manifest exists, then restart the game.

### Fatal Error or Assertion
The error handler **copies the stack trace to the clipboard**. Before launching again, save the complete stack trace and `overdub.log`, then note what happened immediately before the crash.
Testing once without the newest mod can help identify whether the problem belongs to Overdub or the mod.

## Reporting Problems
Search existing issues first. Problems caused only by a third-party mod should normally be reported to its author.
Include:
- What happened and how to reproduce it.
- Environment details and installed mods.
- Whether the issue remains without third-party mods.
- The complete stack trace and `overdub.log` for crashes.
Screenshots or video are useful when they show something the log cannot. Report one problem per issue, and do not upload game files or another author's full mod package.

## Known Limitations
- Mod metadata and assets are loaded at startup.
- Mounted assets cannot be freely unloaded.
- DLL reload is safe only when the mod cleans up all of its state.
- The built-in inspection tools are read-only.

## Mod Development
See the [Mod Development Guide](docs/MOD_DEVELOPMENT.md) for package format, the native API, Unreal helpers, etc.

## Building From Source
### Windows
Open `overdub.sln` in Visual Studio 2022, select `Release` and `x64`, then build the solution.
Output:
```
build/overdub.dll
```
The project has a post-build copy step for a Steam installation path. Change or disable it when the game is installed elsewhere.

### Linux
Install `make` and MinGW-w64, then cross-compile the Windows DLL:
```sh
make release
```
Use `make debug` for a debug build. The output is `build/overdub.dll`.

## Uninstalling
Close the game and remove `overdub.dll` together with `XAPOFX1_5.dll` or `dsound.dll`.
Remove `mods`, `overdub-config.ini`, and `overdub.log` only when you also want to delete installed mods and saved settings.

## License
See [LICENSE](LICENSE).
Overdub is an unofficial project and is not affiliated with the developers or publishers of Hi-Fi RUSH.

