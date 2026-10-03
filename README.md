# Less Wildlife

Sets ordinary wild-boar herds and wolf packs to a configurable percentage of their original size. The default is 40%, rounded to the nearest animal, with at least one animal wherever the original population was positive. Quest and named variants, other creatures, health and damage keep their normal values.

One population hook handles both species. Completed areas are cached, with no background population scans. Changes happen on area encounters before the game's population callback. Unusually large areas or dense same-frame bursts retain their current population values instead of delaying spawning; they can be handled on a later overlap.

## Requirements

- The Blood of Dawnwalker.
- Dawnwalker-compatible UE4SS with native population hooks, delayed game-thread callbacks, a frame counter and the owned soft-reference path API.
- Mod Setting Menu 1.0.6 or newer is optional for the in-game settings. Enable `HookProcessConsoleExec = 1` in your UE4SS loader profile for live Apply.

## Installation

- Vortex: Install Less-Wildlife.zip through Vortex, enable it and deploy.
- Manual: Copy the archive's Data/LessWildlife folder into The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods, preserving the folder structure.

## Population settings

Use Mod Settings → Less Wildlife → Population remaining and Apply. One setting controls both species, from 1% to 100%; 40% is the default. Changes take effect as areas are encountered again. Already spawned animals are unaffected. Setting 100% restores the original values for entries still controlled by this mod; changes made by the game or another mod are preserved.

Without the menu, close the game and set `populationPercent = 40` to your chosen integer from 1 to 100 under `[LessWildlife]` in the mod's `settings.ini`. This file is created on first launch and is not bundled in the archive.

## Logging

Logging defaults to Off. Use Mod Settings → Less Wildlife → Logging and Apply to change it while playing. Without the menu, close the game and set `debugLogging = 1` under `[LessWildlife]` in the mod's `settings.ini`. The file is created on the first launch and is not bundled in the archive.

Details appear in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Summaries count encountered and cached areas, inspected entries, changed boar/wolf entries, deferred areas and failures, with the largest callback duration. Summaries are limited to one per ten seconds of activity. Turn Logging Off after troubleshooting.

## Credits

The 40% preset, species selection and original Less Boars / Less Wolves behavior come from MrSecondPlayer's mods. This local combined implementation replaces their two runtime scripts. Original references and byte provenance are in `UPSTREAM.json`. See `LICENSE.txt` for local-use and attribution information.
