# Changelog

- Choose Off, Error, Warning, Info or Debug logging; Warning is the default.

## Unreleased

- Reduce repeated work when revisiting wildlife areas and leaving groups unchanged.
- Add optional diagnostics to help investigate encounter-related stutters.
- Add Regular guards as a separate hostile replacement choice for boars and wolves.
- Restore missing weapons on replacement bandits and blood guards.
- Give replacement blood guards their hostile behavior from their first spawn.
- Fix enemy replacement failing to start and leaving boars and wolves unchanged despite enabled settings.
- Add separate 0–100% replacement chances for boar herds and wolf packs.
- Choose Bandits, Blood guards, Vidmo, Kobolds and No spawn independently for each species. Enabled outcomes have equal chances; an empty selection keeps the animals.
- Keep each group's result through travel and save/load, with a new roll on natural respawn. Settings changes affect subsequent cycles.
- Let No spawn leave a group empty for one cycle while preserving its later respawn.
- Keep selected group sizes and exclude quest-gated and scripted encounters.
- Fix replacement humanoids inheriting animal poses or unrelated appearances, including when a respawn reuses an old group.
- Include ordinary brown, white and astral wolves.
- Add missing menu settings while preserving valid preferences and consolidating duplicate keys.
- Set Population from 10% to 200%, with independent boar and wolf adjustments and 100% as the default.
- Add optional Logging for replacement, save decisions and next-day respawn conditions.

## 0.0.0

- Combine ordinary boar and wolf reductions into one shared population hook and cache.
- Add Population remaining in Mod Settings, from 1% to 100% with a 40% default. Recalculate from original values on later encounters without compounding reductions.
- Remove full-world startup scans, duplicate entry traversal and per-area update messages during normal play.
- Read owned soft-reference paths directly instead of calling the engine's string conversion for every entry.
- Bound new-area work and cache size; keep unusually large or excess same-frame areas under game control.
- Preserve already reduced entries during a partial-failure retry and recover after delayed hook readiness.
- Add optional Logging with aggregated counts and callback timings.

- Add separate Reduce boars and Reduce wolves On/Off settings, with original-population restoration on later encounters when Off.
