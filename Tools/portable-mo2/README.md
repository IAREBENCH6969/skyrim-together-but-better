# Portable Mod Organizer 2 package

Scripts for building a ready-to-play package of this co-op build for friends: a portable
[Mod Organizer 2](https://github.com/ModOrganizer2/modorganizer) with the build installed, ticked and in load order.

The package never contains game files or SKSE64 (its authors don't allow re-uploading it).
[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) by meh321
may be included unmodified with credit, as its Nexus permissions allow.

## Files

| File | Goes to in the package |
|---|---|
| `SETUP.bat` | `1 - SETUP (run me first).bat` at the root |
| `setup.ps1` | `setup\setup.ps1` |
| `ModOrganizer.template.ini` | `setup\ModOrganizer.template.ini` |
| `README-package.txt` | `README - START HERE.txt` at the root |

## What setup does on the player's PC

1. Finds Skyrim Special Edition (Steam registry key, Steam libraries, or a folder picker) and checks it is 1.7.104.
2. Installs SKSE64 from the archive the player downloads from its official Nexus page
   (loader and DLL next to `SkyrimSE.exe`, scripts as an MO2 mod).
3. Offers to move stray SKSE plugin DLLs out of the game's `Data\SKSE\Plugins`. Skyrim Together loads every SKSE
   plugin, and some (for example PrismaUI) then fail with "failed to open address library file".
4. Checks Address Library: right file for the game version (`versionlib-1-7-104-0.bin`), ticked, correct folder layout.
5. Points Skyrim Together's saved game folder (`HKCU\Software\TiltedPhoques\TiltedEvolution\Skyrim Special Edition`)
   at the same folder MO2 uses, and passes `--exePath` to `SkyrimTogether.exe`, so MO2's virtual Data folder applies.
6. Writes `ModOrganizer.ini` from the template with this PC's paths and starts MO2.

## Building the package

1. Copy a portable MO2 2.5.x install (program files only: no `mods`, `profiles`, `downloads`, `logs`, `webcache`,
   `Stock Game` or `ModOrganizer.ini`). Keep `portable.txt`.
2. Add `mods\Skyrim Together Reborn - Coop Test Build` (the built client), optionally
   `mods\Address Library for SKSE Plugins` with a credits file, and a `profiles\Co-op` profile
   (`modlist.txt`, `plugins.txt`, `loadorder.txt`, local INIs).
3. Add the four files above and `source\coop-changes.patch` (GPL source offer).
4. Zip with `tar.exe -a -c -f "<name>.zip" "<folder>"` (the .NET ZipFile API writes backslash paths).
