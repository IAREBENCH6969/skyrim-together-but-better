SKYRIM TOGETHER REBORN - CO-OP TEST BUILD (ready-made Mod Organizer 2)
=====================================================================

This folder is a complete, portable Mod Organizer 2 (MO2) with the co-op test build already
installed, ticked and in the right load order. It does NOT contain any Skyrim game files:
you need your own copy of the game.

Everyone who plays together, and the server, must use THIS build. It cannot connect to normal
Skyrim Together Reborn players or servers.


WHAT YOU NEED FIRST
-------------------
- Skyrim Special Edition or Anniversary Edition on Steam, updated to game version 1.7.104.
  Start it once from Steam (to the main menu) so it finishes setting itself up, then quit.
- About 1 GB of free disk space for this folder.


INSTALL (about 5 minutes)
-------------------------
1. Unzip this zip somewhere short with no special permissions, for example C:\Games\
   (NOT inside Program Files, and NOT inside your Skyrim folder).
   You should end up with a folder called "Skyrim Together Coop".

2. Double-click "1 - SETUP (run me first).bat" in that folder.
   It finds your Skyrim folder (or asks you to pick it), checks the game version,
   and sets up Mod Organizer for your PC. If Windows SmartScreen warns you, click
   "More info" -> "Run anyway".

3. SKSE (the script extender) is the ONE thing not included: its authors forbid sharing it.
   Setup handles it for you: it opens the SKSE page on Nexus, you click "Manual download"
   under Main files (free Nexus account needed), wait for it to finish, and press Enter in
   the setup window. Setup then installs it and opens Mod Organizer.
   (Already downloaded it? Leave the file in your Downloads folder and setup picks it up.)

4. Address Library is already included and ticked. Nothing to do.

5. In the drop-down at the top right of Mod Organizer, pick "Skyrim Together (Coop Test Build)"
   and press Run. Always start the game this way from now on.

If you move the folder later, just run "1 - SETUP (run me first).bat" again
(close Mod Organizer first).


"FAILED TO LOAD SKYRIM ADDRESS LIBRARY"?
---------------------------------------
Close Mod Organizer and run "1 - SETUP (run me first).bat" again: it checks Address Library
and tells you which of these it is.
- Look at the "Looking for it here:" line in the error. It must be the same Skyrim folder that
  setup found. If it's a different folder (an old Skyrim copy, another modlist's "Stock Game"),
  Skyrim Together is using a folder Mod Organizer doesn't manage. Running setup again fixes that.
- Started SkyrimTogether.exe directly? Always start it from Mod Organizer's Run button (step 5),
  otherwise the game can't see any mods.
- Address Library not ticked in Mod Organizer's left list -> tick it.
- Wrong/old Address Library file: in Mod Organizer right-click Address Library -> Open in Explorer.
  You must see SKSE\Plugins\versionlib-1-7-104-0.bin . If there's no 1-7-104 file, download the
  newer Address Library file from https://www.nexusmods.com/skyrimspecialedition/mods/32444. If the file is there but inside an extra folder (like Data\SKSE\... or
  All in one\SKSE\...), remove the mod and install the zip again.
- Game not on 1.7.104: update Skyrim in Steam (setup shows your version).
- Error box titled with some other .dll (for example PrismaUI.dll) saying "REL/ID.h ... failed to open
  address library file"? That's an old SKSE plugin left in your Skyrim folder's Data\SKSE\Plugins
  from earlier modding, not part of this build. Run setup again and say Y when it offers to move
  them aside. If you added that mod inside Mod Organizer, untick it.



PLAYING TOGETHER
----------------
There are two ways to connect. Steam join is the easiest (no router setup).

A) STEAM JOIN (friends on Steam, no port forwarding)
  Host:
  1. Run mods\Skyrim Together Reborn - Coop Test Build\SkyrimTogetherReborn\SkyrimTogetherServer.exe
     and leave it open.
  2. Start the game through Mod Organizer, load a save (get all the way into the world).
  3. Press F2 (or Right Ctrl) and connect to 127.0.0.1 port 10578.
     ONLY NOW does "Join Game" appear for your friends - not before you are connected.
  Friend:
  4. Start the game through Mod Organizer too and stay at the main menu (or load a save).
     Steam join only works while Skyrim Together is already running - starting it from the
     Steam friends list would launch plain Skyrim instead.
  5. In the Steam friends list (Steam app on the desktop, or Shift+Tab in game),
     right-click the host -> "Join Game".
     After loading into the world you are connected automatically.
  Both of you must run Skyrim from Steam (not GOG) and be Steam friends.

B) DIRECT IP
  - Host runs SkyrimTogetherServer.exe and forwards port 10578 (UDP) on their router to their PC.
  - Host connects to 127.0.0.1, friends connect to the host's public IP (search "what is my ip"),
    port 10578, with F2 in game.
  - No router access? A free LAN-over-internet tool (e.g. Tailscale or ZeroTier) works too:
    friends then connect to the host's address shown in that tool.

Server settings are in ...\SkyrimTogetherReborn\config\STServer.ini.
Use the player list in the F2 overlay to form a party.

GOOD TO KNOW
------------
- The game starts in borderless window at 1920x1080. To change it: in Mod Organizer click the
  puzzle-piece/tools icon -> INI Editor -> skyrimprefs.ini and edit iSize W / iSize H
  (or bFull Screen=1 for fullscreen).
- Saves are kept inside this folder (profiles\Co-op\saves), separate from your normal Skyrim saves.
- Creation Club / Anniversary content you own is used automatically; anything you don't own is
  just skipped.
- Already have Mod Organizer? That's fine: this one is separate and won't touch your other setups.


REPORTING A BUG
---------------
Tell us what you were doing, and send the log from:
  mods\Skyrim Together Reborn - Coop Test Build\SkyrimTogetherReborn\logs\tp_client.log
  (or overwrite\SkyrimTogetherReborn\logs\ - whichever has the newest one)
If the game crashed, also send the crash_*.dmp file next to SkyrimSE.exe in your Skyrim folder.
Note: logs contain your PC's folder names. Look them over first if that bothers you.


LICENSES AND SOURCE
-------------------
- Mod Organizer 2 is free software (GPL v3): https://github.com/ModOrganizer2/modorganizer
  Its third-party licenses are in the licenses folder.
- Skyrim Together Reborn (Tilted Online) is free software under GPL v3:
  https://github.com/tiltedphoques/TiltedEvolution . The changes in this build are in
  source\coop-changes.patch in this folder (apply to upstream commit 13ff0b5, branch "dev").
- Address Library for SKSE Plugins by meh321 is included unmodified, with credit, as its Nexus
  upload permission allows: https://www.nexusmods.com/skyrimspecialedition/mods/32444
- SKSE64 (ianpatt, behippo, scruggsywuggsy the ferret, purple lunchbox) is NOT included: its authors
  do not allow re-uploading. Setup installs it from the file you download from its official page.
