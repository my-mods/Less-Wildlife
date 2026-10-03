# Less Wildlife

[Published 0.0.0 download](https://github.com/my-mods/Less-Wildlife/releases/tag/v0.0.0)

Sets ordinary wild-boar herds and wolf packs to a configurable percentage of their original size. Choose from 10% to 200%: 100% is the normal population and the default, while 200% doubles it. Quantities are rounded to the nearest animal, with at least one animal wherever the original population was positive. Quest and named variants, other creatures, health and damage keep their normal values.

One population hook handles both species. Completed areas are cached, with no background population scans. Changes happen on area encounters before the game's population callback. Unusually large areas or dense same-frame bursts retain their current population values instead of delaying spawning; they can be handled on a later overlap.

## Requirements

- The Blood of Dawnwalker.
- Dawnwalker-compatible UE4SS with native population hooks, delayed game-thread callbacks, a frame counter and the owned soft-reference path API.
- Mod Setting Menu 1.0.6 or newer is optional for the in-game settings. Enable `HookProcessConsoleExec = 1` in your UE4SS loader profile for live Apply.

## Installation

- Vortex: Install Less-Wildlife.zip through Vortex, enable it and deploy.
- Manual: Copy the archive's Data/LessWildlife folder into The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods, preserving the folder structure.

## Population settings

Use Mod Settings > Less Wildlife > Population and Apply. Population ranges from 10% to 200% in 1% steps; 100% is normal and the default. Adjust boars and Adjust wolves independently switch population scaling On or Off, both defaulting to On. Off restores that species' original values still controlled by this mod; it does not remove the animals. Changes take effect as areas are encountered again. Already spawned animals are unaffected. Setting 100% restores the original values for entries still controlled by this mod; changes made by the game or another mod are preserved.

Without the menu, close the game and set `populationPercent = 100` to your chosen integer from 10 to 200 under `[LessWildlife]` in the mod's `settings.ini`. Set `reduceBoars = 0` or `reduceWolves = 0` to turn off scaling for that species, or `1` to turn it on. Missing toggle keys default to On. Existing percentages from 10 to 100 retain their meaning; older values from 1 to 9 are treated as 10 without rewriting the file. This file is created on first launch and is not bundled in the archive.

## Logging

Logging defaults to Off. Use Mod Settings > Less Wildlife > Logging and Apply to change it while playing. Without the menu, close the game and set `debugLogging = 1` under `[LessWildlife]` in the mod's `settings.ini`. The file is created on the first launch and is not bundled in the archive.

Details appear in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Summaries count encountered and cached areas, inspected entries, changed boar/wolf entries, deferred areas and failures, with the largest callback duration. Summaries are limited to one per ten seconds of activity. Turn Logging Off after troubleshooting.

## Credits

Inspired by [Less Boars](https://www.nexusmods.com/thebloodofdawnwalker/mods/326) and [Less Wolves](https://www.nexusmods.com/thebloodofdawnwalker/mods/318) by MrSecondPlayer, including their 40% preset and ordinary-species selection. Less Wildlife uses a custom implementation; the original scripts and artwork are not included. The optional Mod Setting Menu helper is included unchanged under its integration guide's copying permission. References and byte provenance are in `UPSTREAM.json`; rights and attribution are in `LICENSE.txt`.

## Build from source

No compilation is needed for the Lua payload. Create a ZIP with the contents
of `package/` at its root, then add `LICENSE.txt`, `CHANGELOG.md`,
`RELEASE-NOTES.md` and `UPSTREAM.json` at the same root. Name the result
`Less-Wildlife.zip`. The archive root must contain `Data`, `mod.manifest` and
`README.txt`; do not include the enclosing `package` directory or a personal
`settings.ini`.
