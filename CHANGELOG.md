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
