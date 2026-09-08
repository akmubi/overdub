# Troubleshooting

Start with `overdub.log`. It records loader startup, package parsing, DLL errors, Lua errors, and asset mount failures.

## Overdub does not load

Check that `overdub.dll` and the correct bootstrap DLL are beside the game executable. Steam and Epic use `XAPOFX1_5.dll`; Xbox and Game Pass use `dsound.dll`.

Remove UE4SS and other proxy DLL loaders from the same directory. Make sure the Overdub release supports the installed game build.

If the game runs but does not create `overdub.log` or `mods`, the bootstrap did not load Overdub.

## A window does not open

The mod manager uses the backtick key by default, not the apostrophe key. A keyboard layout or another application may intercept it. Change the shortcut in `overdub-config.ini` while the game is closed if the Settings window cannot be reached.

Wait until the game has finished its first loading sequence before deciding that the overlay failed.

## A mod is missing

Each mod needs its own directory and a valid `mod.ini`. Its ID must be unique. Restart the game after adding the directory because package discovery happens at startup.

Manifest errors are written to `overdub.log`.

## A native mod fails

Open the mod's Code section and read the error. Common causes are a missing DLL, a wrong `path`, an incompatible API version, or a failed `init` callback.

A reload can fail when the old DLL left a thread or unmanaged callback running. Restart the game before assuming that the newly built DLL is broken.

## A Lua mod fails

The log includes the Lua message and stack trace. Confirm that the manifest selects the intended runtime. Native Overdub Lua is the default. Existing UE4SS scripts must set `runtime = ue4ss`.

Module names use Lua's `require` rules. Check the entry directory, the mod directory, the module filename, and the exported name of any C module.

## An asset mod fails

Check the Assets section and the log for mount errors. A `.utoc` file needs its matching `.ucas` file. Package paths and cooked target versions must match the game.

Asset changes and mod order changes require a restart.

## The game crashes

Overdub's error handler copies the stack trace to the clipboard. Save it before launching the game again. Also keep `overdub.log` and note the last action that reproduced the crash.

Test once without the newest third-party mod. If the crash belongs to a mod, report it to that mod's author. A useful report contains the exact steps, game store and version, Overdub version, installed mod list, stack trace, and log. Native mod authors should keep the matching PDB files.
