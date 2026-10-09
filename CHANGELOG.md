## 0.2.0-dev

- Set a separate 1–10 member range for Bandits, Regular guards, Blood guards, Vidmo and Kobolds. Equal limits give a fixed group size.
- Keep each replacement group’s chosen size through travel and save loading; new natural cycles use the current ranges.
- Rename Population to Base wildlife group size and use it only for groups that remain wildlife.

## 0.1.0

- Add separate 0–100% replacement chances for boar herds and wolf packs.
- Choose Bandits, Blood guards, Regular guards, Vidmo, Kobolds or No spawn independently for each species. Enabled outcomes have equal chances; an empty selection keeps the animals.
- Keep each group’s choice through travel and save loading, with a new roll on natural respawn. No spawn leaves the group empty for one cycle.
- Keep quest-gated and scripted encounters unchanged.
- Fix replacement enemies failing to appear, missing their weapons, inheriting animal poses or behaving as non-hostile creatures.
- Choose Off, Error, Warning, Info or Debug logging; Warning is the default.

# Changelog

## Unreleased

- Expand Population to 10–200%, with 100% as the normal population and new default.
- Allow positive populations of one animal to grow to two at 200%.
- Rename the independent species switches to Adjust boars and Adjust wolves; Off restores owned original values.
- Preserve existing 10–100% preferences and treat older 1–9% values as 10%.

## 0.0.0

- Combine ordinary boar and wolf reductions into one shared population hook and cache.
- Add Population remaining in Mod Settings, from 1% to 100% with a 40% default. Recalculate from original values on later encounters without compounding reductions.
- Remove full-world startup scans, duplicate entry traversal and per-area update messages during normal play.
- Read owned soft-reference paths directly instead of calling the engine's string conversion for every entry.
- Bound new-area work and cache size; keep unusually large or excess same-frame areas under game control.
- Preserve already reduced entries during a partial-failure retry and recover after delayed hook readiness.
- Add optional Logging with aggregated counts and callback timings.

- Add separate Reduce boars and Reduce wolves On/Off settings, with original-population restoration on later encounters when Off.
