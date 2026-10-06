# Less Wildlife

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/711?tab=files)

Sets ordinary wild-boar herds and wolf packs to a configurable percentage of their original size. Choose from 10% to 200%: 100% is the normal population and the default, while 200% doubles it. Quantities are rounded to the nearest animal, with at least one animal wherever the original population was positive. Quest and named variants, other creatures, health and damage keep their normal values.

Give boars and wolves separate replacement chances from 0% to 100%. Enable Bandits, Blood guards, Vidmo, Kobolds or No spawn independently for each species. One roll chooses the result for the whole herd or pack. Supported wolves include brown, white and astral variants; quest-specific, boss and summoned definitions are excluded.

Replacement chances default to 0%. Enemy choices default to On and No spawn defaults to Off. Population adjustment remains independent of replacement. The mod uses population events without background world scans. Unusually large areas or dense loading bursts may retain their current values.

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

## Boar and wolf replacement

Open Mod Settings > Less Wildlife. Under Boars or Wolves, set Replacement chance and toggle the allowed outcomes, then Apply. Each enemy toggle adds that type to the species' random pool. All enabled outcomes have equal weight. For example, 60% with Bandits and Kobolds enabled leaves about 40% of groups as animals, 30% as bandits and 30% as kobolds. A single enabled outcome receives every successful replacement roll. An empty pool leaves the animals.

The chosen result belongs to the whole group and is stored with that game save. Travel, repeated overlap and loading the save reuse it. Settings changes apply to newly encountered groups and subsequent natural respawn cycles. Partial kills do not reroll the survivors. Group size retains the existing Population setting.

No spawn makes the selected encounter empty for its current cycle while retaining the game's respawn schedule. It does not permanently remove the group. A later natural cycle rolls again using the current settings.

Only wildlife entries with random spawn locations, an encounter role and next-day respawning are eligible. Quest-gated encounters, authored AI overrides and fixed scripted spawn points are excluded. Existing creatures are not destroyed and replaced. Replacement enemies are hostile toward the player and use their own combat AI, equipment and loot. Missing weapons on existing replacement bandits and blood guards are restored once; later reloads preserve changes to their equipment. Generated roaming activities clear the animal sleeping, sitting and eating montages that caused humanoids to start in awkward poses. Humanoid appearance initialization also clears inherited animal coat data and resets reused appearance fields when a new cycle chooses another enemy.

For manual configuration, set `boarReplacementChance` and `wolfReplacementChance` to integers from 0 to 100. Each species has five 0/1 switches: `boarBandits`, `boarBloodGuards`, `boarVidmo`, `boarKobolds`, `boarNoSpawn`, and the matching `wolf` names. A value of 1 includes that outcome; 0 excludes it. Close the game before editing `settings.ini`.

These encounters follow the game's next-day respawn schedule. Defeat every member, leave the area, let the game advance into the next day and return. Dead members inside the population visibility region can delay respawn, and cleanup can require another attempt. Loading a save or briefly leaving and returning does not itself start a new cycle.

## Logging

Logging defaults to Off. Use Mod Settings > Less Wildlife > Logging and Apply to change it while playing. Without the menu, close the game and set `debugLogging = 1` under `[LessWildlife]` in the mod's `settings.ini`. The file is created on the first launch and is not bundled in the archive.

Details appear in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Population summaries include counts, deferred areas, failures and callback timings. Replacement messages report saved decisions, appearance resets, suppression, exclusions and failed validation. Respawn messages distinguish next-day scheduling, visibility delays, dead-member cleanup and refill attempts. A successful attempt message alone does not confirm that actors appeared.

Examples and repeated messages are limited, with aggregate reports at most once per ten seconds of activity. Logging Off skips optional diagnostics and timing. Save and encounter processing remains active. If any required native function, layout or hook cannot be validated, replacement is unavailable while the Lua population controls remain available. Missing-object and missing-function errors identify the required binding. Turn Logging Off after troubleshooting.

## Credits

Inspired by [Less Boars](https://www.nexusmods.com/thebloodofdawnwalker/mods/326) and [Less Wolves](https://www.nexusmods.com/thebloodofdawnwalker/mods/318) by MrSecondPlayer, including their 40% preset and ordinary-species selection. Less Wildlife uses a custom implementation; the original scripts and artwork are not included. The optional Mod Setting Menu helper is included unchanged under its integration guide's copying permission. References and byte provenance are in `UPSTREAM.json`; rights and attribution are in `LICENSE.txt`.

## Build from source

Build the native helper with the pinned dependencies described in
`native/BUILD.md`, then place `main.dll` in `package/Data/LessWildlife/dlls`.
Create `Less-Wildlife.zip` with the contents of `package/` at its root,
plus `LICENSES/`, `Nexus/`, `LICENSE.txt`, `CHANGELOG.md`, `RELEASE-NOTES.md`
and `UPSTREAM.json`. The root must contain `Data`, `mod.manifest` and
`README.txt`; exclude personal settings, build directories and development tools.
