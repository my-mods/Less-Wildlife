# Wilder Wildlife - Customize Enemy Spawns

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/711?tab=files)

Sets ordinary wild-boar herds and wolf packs to a configurable percentage of their original size. Choose from 10% to 200%: 100% is the normal population and the default, while 200% doubles it. Quantities are rounded to the nearest animal, with at least one animal wherever the original population was positive. Quest and named variants, other creatures, health and damage keep their normal values.

Give boars and wolves separate replacement chances from 0% to 100%. Enable Bandits, Regular guards, Blood guards, Vidmo, Kobolds or No spawn independently for each species. One roll chooses the result for the whole herd or pack. Supported wolves include brown, white and astral variants; quest-specific, boss and summoned definitions are excluded.

Replacement chances default to 0%. Enemy choices default to On and No spawn defaults to Off. Population adjustment remains independent of replacement. The mod uses population events without background world scans. Unusually large areas or dense loading bursts may retain their current values.

## Requirements

- The Blood of Dawnwalker.
- Required: [UE4SS for Dawnwalker by Vercadi](https://www.nexusmods.com/thebloodofdawnwalker/mods/18) **1.3 (RC6) or later**, with C++ mod support, native population hooks, delayed game-thread callbacks, a frame counter and the owned soft-reference path API. If the replacement helper cannot validate the population code or layouts, the existing population controls remain available.
- Mod Setting Menu 1.0.6 or newer is optional for the in-game settings. Enable `HookProcessConsoleExec = 1` in your UE4SS loader profile for live Apply.

## Installation

- Vortex: Install Less-Wildlife.zip through Vortex, enable it and deploy.
- Manual: Copy the archive's Data/LessWildlife folder into The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods, preserving the folder structure.

## Wildlife group size

Use Mod Settings > Less Wildlife > Wildlife > Base wildlife group size and Apply. This controls members within groups that remain wildlife, independently of replacement enemy counts. It ranges from 10% to 200% in 1% steps; 100% is normal and the default. Adjust boars and Adjust wolves independently switch population scaling On or Off, both defaulting to On. Off restores that species' original values still controlled by this mod; it does not remove the animals. Changes take effect as areas are encountered again. Already spawned animals are unaffected. Setting 100% restores the original values for entries still controlled by this mod; changes made by the game or another mod are preserved.

Without the menu, close the game and set `populationPercent = 100` to your chosen integer from 10 to 200 under `[LessWildlife]` in the mod's `settings.ini`. Set `reduceBoars = 0` or `reduceWolves = 0` to turn off scaling for that species, or `1` to turn it on. Missing settings are added on startup. Existing valid preferences, comments and unrelated sections are retained; repeated known settings are consolidated using the last valid value. Older percentages from 1 to 9 become 10 so the menu can open them. The file is created on first launch and is not bundled in the archive.

## Enemy group sizes

Under Enemy group sizes, choose minimum and maximum members for each enemy type. Both limits accept integers from 1 to 10 and apply equally to boar and wolf replacements. Every count between the limits has the same chance. Set both limits to 1 for a single enemy, or both to any other count for a fixed group size. Reversed limits are interpreted in ascending order.

Defaults: Bandits 3–6, Regular guards 2–4, Blood guards 2–4, Vidmo 1–2 and Kobolds 4–10. These ranges override Base wildlife group size. No spawn remains a separate outcome.

The chosen count is saved with the group's enemy type. Travel, reloads, partial kills and changing settings do not reroll or refill an existing group. New groups and later natural respawn cycles use the current ranges. Groups already saved by an earlier version retain their previous sizing for the current cycle. The game still controls encounter locations, navigation and respawn timing.

For manual configuration, use the paired keys `banditGroupMin`/`banditGroupMax`, `guardGroupMin`/`guardGroupMax`, `bloodGuardGroupMin`/`bloodGuardGroupMax`, `vidmoGroupMin`/`vidmoGroupMax` and `koboldGroupMin`/`koboldGroupMax`. Close the game before editing settings.ini. Integer limits outside 1–10 are clamped; missing or non-integer limits use defaults.

## Boar and wolf replacement

Open Mod Settings > Less Wildlife. Under Boars or Wolves, set Replacement chance and toggle the allowed outcomes, then Apply. Regular guards are standard longsword soldiers made hostile toward the player. Each enemy toggle adds that type to the species' random pool. All enabled outcomes have equal weight. For example, 60% with Bandits and Kobolds enabled leaves about 40% of groups as animals, 30% as bandits and 30% as kobolds. A single enabled outcome receives every successful replacement roll. An empty pool leaves the animals.

The chosen result belongs to the whole group and is stored with that game save. Travel, repeated overlap and loading the save reuse it. Settings changes apply to newly encountered groups and subsequent natural respawn cycles. Partial kills do not reroll the survivors. Replacement groups use their enemy-specific size range, independently of Base wildlife group size.

No spawn makes the selected encounter empty for its current cycle while retaining the game's respawn schedule. It does not permanently remove the group. A later natural cycle rolls again using the current settings.

Only wildlife entries with random spawn locations, an encounter role and next-day respawning are eligible. Quest-gated encounters, authored AI overrides and fixed scripted spawn points are excluded. Existing creatures are not destroyed and replaced. Replacement enemies are hostile toward the player and use their own combat AI, equipment and loot. Missing weapons on existing replacement bandits, regular guards and blood guards are restored once; later reloads preserve changes to their equipment. Generated roaming activities clear the animal sleeping, sitting and eating montages that caused humanoids to start in awkward poses. Humanoid appearance initialization also clears inherited animal coat data and resets reused appearance fields when a new cycle chooses another enemy.

For manual configuration, set `boarReplacementChance` and `wolfReplacementChance` to integers from 0 to 100. Each species has six 0/1 switches: `boarBandits`, `boarGuards`, `boarBloodGuards`, `boarVidmo`, `boarKobolds`, `boarNoSpawn`, and the matching `wolf` names. A value of 1 includes that outcome; 0 excludes it. Close the game before editing `settings.ini`.

These encounters follow the game's next-day respawn schedule. Defeat every member, leave the area, let the game advance into the next day and return. Dead members inside the population visibility region can delay respawn, and cleanup can require another attempt. Loading a save or briefly leaving and returning does not itself start a new cycle.

## Logging

Logging is the final menu setting: Off, Error, Warning (default), Info or Debug. Levels include all more severe messages. Use Debug to record detailed encounter, size, equipment and timing information in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Off silences this mod. Without the menu, close the game and set `logLevel` to 0–4 in settings.ini.

Diagnostics are bounded and aggregated. Levels below Debug skip optional timings and detailed instrumentation; saved encounter processing remains active. Successful offline checks or spawn attempts alone do not prove gameplay behavior or a frame-rate improvement.

## Credits

Inspired by [Less Boars](https://www.nexusmods.com/thebloodofdawnwalker/mods/326) and [Less Wolves](https://www.nexusmods.com/thebloodofdawnwalker/mods/318) by MrSecondPlayer, including their 40% preset and ordinary-species selection. Wilder Wildlife uses a custom implementation; the original scripts and artwork are not included. The optional Mod Setting Menu helper is included unchanged under its integration guide's copying permission. References and byte provenance are in `UPSTREAM.json`; rights and attribution are in `LICENSE.txt`.

## Build from source

Build the native helper with the pinned dependencies described in
`native/BUILD.md`, then place `main.dll` in `package/Data/LessWildlife/dlls`.
Create `Less-Wildlife.zip` with the contents of `package/` at its root,
plus `LICENSES/`, `Nexus/`, `LICENSE.txt`, `CHANGELOG.md`, `RELEASE-NOTES.md`
and `UPSTREAM.json`. The root must contain `Data`, `mod.manifest` and
`README.txt`; exclude personal settings, build directories and development tools.

## Performance and diagnostics

Repeated encounter-name checks reuse a bounded cache of parsed names. Groups that stay as wildlife or choose No spawn skip unused replacement-profile preparation. Existing area handles are reused. Saved choices, quest exclusions, spawn probabilities and respawn rules retain their normal behavior.

Logging includes aggregate elapsed times for table building, appearance initialization, activity binding, encounter queueing, eligibility, attempts, clock updates and point generation. These hook measurements include the original game call and may be nested; they are not isolated mod overhead or frame times. Logging below Debug skips this optional instrumentation. Saved encounter choices, quest exclusions and spawn probabilities are unchanged.

Choose **Debug** in the final Logging setting when collecting detailed diagnostics in `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Warning is the default for normal play. Timings and offline checks do not establish an in-game frame-rate improvement.
