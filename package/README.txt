# Wilder Wildlife - Customize Enemy Spawns

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/711?tab=files)

Sets ordinary wild-boar herds and wolf packs to a configurable percentage of their original size. Choose from 10% to 200%: 100% is the normal population and the default, while 200% doubles it. Quantities are rounded to the nearest animal, with at least one animal wherever the original population was positive. Quest and named variants, other creatures, health and damage keep their normal values.

Give boars and wolves separate replacement chances from 0% to 100%. Choose Bandits, Regular guards, Blood guards, Vidmo, Kobolds or No spawn in one shared enemy pool. One roll chooses the result for the whole herd or pack. Supported wolves include brown, white and astral variants; quest-specific, boss and summoned definitions are excluded.

Replacement chances default to 0%. Enemy choices default to On and No spawn defaults to Off. Each species switches between wildlife sizing and replacement mode. The mod uses population events without background world scans. Unusually large areas or dense loading bursts may retain their current values.

## Requirements

- The Blood of Dawnwalker.
- Required: [UE4SS for Dawnwalker by Vercadi](https://www.nexusmods.com/thebloodofdawnwalker/mods/18) **1.3 (RC6) or later**, with C++ mod support, native population hooks, delayed game-thread callbacks, a frame counter and the owned soft-reference path API. If the replacement helper cannot validate the population code or layouts, the existing population controls remain available.
- Mod Setting Menu 1.0.7.1 or newer is optional for the in-game settings. Enable `HookProcessConsoleExec = 1` in your UE4SS loader profile for live Apply.

## Installation

- Vortex: Install Less-Wildlife.zip through Vortex, enable it and deploy.
- Manual: Copy the archive's Data/LessWildlife folder into The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods, preserving the folder structure.

## Wildlife

Open Mod Settings > Less Wildlife > Wildlife. Boars and Wolves each have a mode toggle:

- **Off:** shows Wildlife group size, a separate 10–200% slider for that species. 100% is normal. This changes members within each group, not the number of encounter locations.
- **On:** hides wildlife sizing and shows Replace boars or Replace wolves, the chance from 0–100% to use the shared enemy pool. Groups that remain wildlife use their normal size in this mode.

Both modes default to Off, with wildlife sizes at 100% and replacement chances at 0%. Hidden slider values are retained when switching modes. Apply changes affect later encounters and newly selected natural cycles; existing animals and saved replacement cycles are not rerolled or refilled. The mod restores only values it still owns and preserves changes made by the game or another mod.

Without the menu, close the game and edit settings.ini under [LessWildlife]. Use replaceBoars/replaceWolves (0 for wildlife sizing, 1 for replacement mode), boarPopulationPercent/wolfPopulationPercent (10–200), and boarReplacementChance/wolfReplacementChance (0–100).

Upgrades import the old populationPercent into both species sliders; a formerly disabled species-scaling switch imports 100%. A nonzero previous replacement chance enables replacement mode for that species. Enemies enabled in either old species pool enter the shared pool. Explicit new settings win. Valid preferences, comments and unrelated sections are preserved; missing settings are added once. Old keys remain as import-only history. Personal settings.ini is not included in the archive.

## Enemy group sizes

Under Enemy type, enable an enemy to reveal its Minimum group size and Maximum group size sliders directly beneath it. Both limits accept integers from 1 to 10 and apply equally to boar and wolf replacements. Every count between the limits has the same chance. Set both limits to 1 for a single enemy, or both to any other count for a fixed group size. Reversed limits are interpreted in ascending order.

Defaults: Bandits 3–6, Regular guards 2–4, Blood guards 2–4, Vidmo 1–2 and Kobolds 4–10. These ranges determine replacement sizes independently of the hidden wildlife percentages. No spawn remains a separate outcome.

The chosen count is saved with the group's enemy type. Travel, reloads, partial kills and changing settings do not reroll or refill an existing group. New groups and later natural respawn cycles use the current ranges. Groups already saved by an earlier version retain their previous sizing for the current cycle. The game still controls encounter locations, navigation and respawn timing.

For manual configuration, use the paired keys `banditGroupMin`/`banditGroupMax`, `guardGroupMin`/`guardGroupMax`, `bloodGuardGroupMin`/`bloodGuardGroupMax`, `vidmoGroupMin`/`vidmoGroupMax` and `koboldGroupMin`/`koboldGroupMax`. Close the game before editing settings.ini. Integer limits outside 1–10 are clamped; missing or non-integer limits use defaults.

## Boar and wolf replacement

Open Mod Settings > Less Wildlife. In Wildlife, turn Boars or Wolves On and set Replace boars or Replace wolves. Choose the shared allowed outcomes in Enemy type, then Apply. Regular guards are standard longsword soldiers made hostile toward the player. Each enemy toggle adds that type to the shared random pool. All enabled outcomes have equal weight. For example, 60% with Bandits and Kobolds enabled leaves about 40% of groups as animals, 30% as bandits and 30% as kobolds. A single enabled outcome receives every successful replacement roll. An empty pool leaves the animals.

The chosen result belongs to the whole group and is stored with that game save. Travel, repeated overlap and loading the save reuse it. Settings changes apply to newly encountered groups and subsequent natural respawn cycles. Partial kills do not reroll the survivors. Replacement groups use their enemy-specific size range.

No spawn makes the selected encounter empty for its current cycle while retaining the game's respawn schedule. It does not permanently remove the group. A later natural cycle rolls again using the current settings.

Only wildlife entries with random spawn locations, an encounter role and next-day respawning are eligible. Quest-gated encounters, authored AI overrides and fixed scripted spawn points are excluded. Existing creatures are not destroyed and replaced. Replacement enemies are hostile toward the player and use their own combat AI, equipment and loot. Missing weapons on existing replacement bandits, regular guards and blood guards are restored once; later reloads preserve changes to their equipment. Generated roaming activities clear the animal sleeping, sitting and eating montages that caused humanoids to start in awkward poses. Humanoid appearance initialization also clears inherited animal coat data and resets reused appearance fields when a new cycle chooses another enemy.

For the shared pool, use enemyBandits, enemyGuards, enemyBloodGuards, enemyVidmo, enemyKobolds and enemyNoSpawn. A value of 1 includes the outcome; 0 excludes it. Both species use this same pool. Close the game before editing settings.ini.

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
