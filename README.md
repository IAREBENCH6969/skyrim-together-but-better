# Skyrim Together But Better

An unofficial build of [Skyrim Together Reborn](https://github.com/tiltedphoques/TiltedEvolution) (TiltedEvolution)
with extra co-op features. All credit for Skyrim Together goes to the Tilted Phoques team; this repository only adds
the changes below on top of their `dev` branch, under the same GPL-3.0 license.

**Added in this build**
- **Trading:** trade items and gold with another player from the F2 player list; both players must accept.
- **Revive:** at 0 health you go down for 30 seconds; a teammate presses E to revive you with one of their healing potions.
- **Shared map markers:** everyone sees each other's custom map marker, in that player's colour.
- **Players on the map and compass:** other players show as coloured arrows on the world map and dots on the compass.
- **Crafted potions and poisons sync** between players.
- **Steam join:** friends join the host's session from the Steam friends list, no port forwarding needed.
- **Party re-invite fix:** a player can be invited again after missing or declining an invite.

Clients built from this repository only connect to servers built from it.
``Tools/portable-mo2`` has the scripts for a ready-to-play portable Mod Organizer 2 package.

## Requirements

- **Skyrim Special Edition or Anniversary Edition on Steam, game version 1.7.104.**
- **[SKSE64](https://www.nexusmods.com/skyrimspecialedition/mods/30379)** (Anniversary Edition build 2.3.1). Its authors don't allow
  re-uploading it, so get it from that page.
- **[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)** by meh321: the newest
  "All in one" file that includes `versionlib-1-7-104-0.bin`. The older "All in one (all game versions)" file stops at 1.6.1170.
- **[Mod Organizer 2](https://github.com/ModOrganizer2/modorganizer/releases)** (recommended).
- Don't run the original Skyrim Together Reborn at the same time: this build replaces it, and the two can't play together.

## Download and install

Download the latest build from the [Releases](https://github.com/IAREBENCH6969/skyrim-together-but-better/releases) page.

1. In Mod Organizer 2, install SKSE64 (loader and DLL next to `SkyrimSE.exe`) and Address Library, and untick the
   original Skyrim Together Reborn if you have it.
2. Install the "Skyrim Together Reborn - Coop Test Build" folder from the download as a mod and tick it, along with
   `SkyrimTogether.esp` and `SkyrimTogetherQuestPatches.esp`.
3. Add `SkyrimTogetherReborn\SkyrimTogether.exe` (inside that mod) as an executable and start the game with it from MO2.
4. Remove other SKSE plugins from your game's `Data\SKSE\Plugins` folder if you get "failed to open address library file".

Hosting and joining (server, F2 overlay, Steam join, direct IP) are described in
[Tools/portable-mo2/README-package.txt](Tools/portable-mo2/README-package.txt), under "Playing together".

---
# Skyrim Together
#### codename "TiltedEvolution"
[![GitHub release](https://img.shields.io/github/v/release/tiltedphoques/TiltedEvolution?color=23314E)](https://github.com/tiltedphoques/TiltedEvolution/releases)
[![Nexus Mods](https://img.shields.io/badge/Nexus%20Mods-download-FB923C?logo=nexusmods&logoColor=white)](https://www.nexusmods.com/skyrimspecialedition/mods/69993)
[![Language](https://img.shields.io/github/languages/top/tiltedphoques/TiltedEvolution?logo=cplusplus&color=F34B7D&logoColor=white)](https://github.com/tiltedphoques/TiltedEvolution)
[![Build status](https://github.com/tiltedphoques/TiltedEvolution/actions/workflows/windows.yml/badge.svg)](https://github.com/tiltedphoques/TiltedEvolution/actions/workflows/windows.yml)
[![Build linux](https://github.com/tiltedphoques/TiltedEvolution/actions/workflows/linux.yml/badge.svg)](https://github.com/tiltedphoques/TiltedEvolution/actions/workflows/linux.yml)
[![Discord](https://img.shields.io/discord/247835175860305931.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/skyrimtogether)

<img src="https://avatars.githubusercontent.com/u/52131158?s=200&v=4" align="right"
     alt="Size Limit logo by Anton Lovchikov" width="110" height="100">

TiltedEvolution is a framework created to enable multiplayer in Bethesda games, currently supporting **Skyrim Special Edition**.

## Getting started
To play Skyrim Together, go to the [nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/69993).

For general information, go to the [TiltedEvolution Wiki](https://wiki.tiltedphoques.com/tilted-online/).

Check out the [build guide](https://wiki.tiltedphoques.com/tilted-online/technical-documentation/build-guide) for setup and development info on the project. When writing code, check the CODE_GUIDELINES.md and make sure to run clang-format!

## Reporting bugs
If you would like to report a bug please report them in the "Issues" tab on this page. Detailed and reproducible bug reports are of great importance for the development of the project.

## Contributing
Have some experience in C++, and want to help advance the project faster? Contribute!
- Check the issues, it's a good place to start when you don't know what to do.
- Fork the repository and create pull requests to this repository.
- Create pull requests to the "dev" branch, not to master.
- Try to keep your code clean, following the code guidelines.

## Main project source tree

* [**client/**](./Code/client): Skyrim SE client: engine hooks, reverse-engineered game types and the sync logic.
* [**immersive_launcher/**](./Code/immersive_launcher): `SkyrimTogether.exe`, which starts the game with the client loaded.
* [**immersive_elf/**](./Code/immersive_elf): `EarlyLoad.dll` helper DLL for the launcher. Detects if Mod Organizer 2 was injected first.
* [**tp_process/**](./Code/tp_process): Worker for the CEF (Chromium Embedded Framework) overlay.
* [**skyrim_ui/**](./Code/skyrim_ui): In-game UI, written in Angular/TypeScript.
* [**server/**](./Code/server): Game server (`STServer.dll`).
* [**server_runner/**](./Code/server_runner): Dedicated server executable that hosts the server DLL.
* [**encoding/**](./Code/encoding): Network message and struct definitions shared by the client and server.
* [**common/**](./Code/common), [**base/**](./Code/base): Shared utilities.
* [**components/**](./Code/components): Shared modules (console/settings, ESP loader, resources, ImGui).
* [**tests/**](./Code/tests): Unit tests.
* [**GameFiles/**](./GameFiles): Plugins, scripts and assets shipped with the mod.
* [**Libraries/**](./Libraries): Networking, hooking, reverse-engineering and UI libraries (git submodules).
* [**Tools/**](./Tools): Reverse-engineering and address scripts.

## License
[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

TiltedEvolution is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
