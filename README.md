# Less Wildlife

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/711?tab=files)

Sets ordinary wild-boar herds and wolf packs to a configurable percentage of their original size. Choose from 10% to 200%: 100% is the normal population and the default, while 200% doubles it. Quantities are rounded to the nearest animal, with at least one animal wherever the original population was positive. Quest and named variants, other creatures, health and damage keep their normal values.

The optional Wolves to bandits setting replaces eligible newly generated ordinary wolf packs with bandits. It defaults to Off and keeps the pack size selected by Population. The first replacement prototype provides this single replacement; boars retain their population adjustment controls.

One shared population hook handles animal counts. A native helper changes wolf encounter definitions before registration, keeping the original encounter row IDs, quantities and respawn policy. There are no background population scans. Unusually large areas or dense bursts retain their current values.

## Requirements

- The Blood of Dawnwalker.
- Required: [UE4SS for Dawnwalker by Vercadi](https://www.nexusmods.com/thebloodofdawnwalker/mods/18) **1.3 (RC6) or later**, with C++ mod support, native population hooks, delayed game-thread callbacks, a frame counter and the owned soft-reference path API. If the replacement helper cannot validate the population code or layouts, the existing population controls remain available.
- Mod Setting Menu 1.0.6 or newer is optional for the in-game settings. Enable `HookProcessConsoleExec = 1` in your UE4SS loader profile for live Apply.

## Installation

- Vortex: Install Less-Wildlife.zip through Vortex, enable it and deploy.
- Manual: Copy the archive's Data/LessWildlife folder into The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods, preserving the folder structure.

## Population settings

Use Mod Settings > Less Wildlife > Population and Apply. Population ranges from 10% to 200% in 1% steps; 100% is normal and the default. Adjust boars and Adjust wolves independently switch population scaling On or Off, both defaulting to On. Off restores that species' original values still controlled by this mod; it does not remove the animals. Changes take effect as areas are encountered again. Already spawned animals are unaffected. Setting 100% restores the original values for entries still controlled by this mod; changes made by the game or another mod are preserved.

Without the menu, close the game and set `populationPercent = 100` to your chosen integer from 10 to 200 under `[LessWildlife]` in the mod's `settings.ini`. Set `reduceBoars = 0` or `reduceWolves = 0` to turn off scaling for that species, or `1` to turn it on. Missing settings are added on startup. Existing valid preferences, comments and unrelated sections are retained; repeated known settings are consolidated using the last valid value. Older percentages from 1 to 9 become 10 so the menu can open them. The file is created on first launch and is not bundled in the archive.

## Wolf replacement

Set Wolves to bandits to On and Apply before entering a new population area. Already generated encounters are left alone. This switch is independent of Adjust wolves: that setting controls quantity, while Wolves to bandits controls the encounter type. The prototype accepts only Off or On, represented by `wolfReplacementChance = 0` or `100` in `settings.ini`.

Only ordinary wolf entries with random spawn locations, an encounter role and next-day respawning are eligible. Entries with authored AI overrides, start conditions or attached guard areas are left alone. Bandits use their normal equipment, AI and loot; existing animals are not destroyed and replaced after spawning.

## Logging

Logging defaults to Off. Use Mod Settings > Less Wildlife > Logging and Apply to change it while playing. Without the menu, close the game and set `debugLogging = 1` under `[LessWildlife]` in the mod's `settings.ini`. The file is created on the first launch and is not bundled in the archive.

Details appear in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Summaries count encountered and cached areas, inspected entries, changed boar/wolf entries, deferred areas and failures, with the largest callback duration. Replacement messages identify converted encounters and skipped or rolled-back changes. Summaries are limited to one per ten seconds of activity. Turn Logging Off after troubleshooting.

## Credits

Inspired by [Less Boars](https://www.nexusmods.com/thebloodofdawnwalker/mods/326) and [Less Wolves](https://www.nexusmods.com/thebloodofdawnwalker/mods/318) by MrSecondPlayer, including their 40% preset and ordinary-species selection. Less Wildlife uses a custom implementation; the original scripts and artwork are not included. The optional Mod Setting Menu helper is included unchanged under its integration guide's copying permission. References and byte provenance are in `UPSTREAM.json`; rights and attribution are in `LICENSE.txt`.

## Build from source

Build the native helper with the pinned dependencies described in
`native/BUILD.md`, then place `main.dll` in `package/Data/LessWildlife/dlls`.
Create `Less-Wildlife.zip` with the contents of `package/` at its root,
plus `LICENSES/`, `Nexus/`, `LICENSE.txt`, `CHANGELOG.md`, `RELEASE-NOTES.md`
and `UPSTREAM.json`. The root must contain `Data`, `mod.manifest` and
`README.txt`; exclude personal settings, build directories and development tools.
