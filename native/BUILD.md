# Building Less Wildlife

Use x64 MSVC with C++23 support, the Windows SDK, CMake 3.25 or newer,
Ninja and Git. Build dependencies are pinned in CMakeLists.txt:

- RE-UE4SS: `97b7e501c19d8b2b7c662feee73aaa0dc1f0a4d1`.
- Its Unreal headers: `eb40a05f49509bdeb1ac39287032b60af585cca8`.
- MinHook, fmt, ImGui, ImGuiColorTextEdit and Zydis use the exact commits
  recorded in CMakeLists.txt. These headers are required by the UE4SS SDK;
  the helper does not create its own UI.

Clone RE-UE4SS with its submodules, check out the revisions above, and run
these commands in an x64 MSVC developer terminal from the repository root:

```text
cmake -S native -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUE4SS_SDK=C:/path/to/RE-UE4SS
cmake --build build
cmake -E make_directory package/Data/LessWildlife/dlls
cmake -E copy build/main.dll package/Data/LessWildlife/dlls/main.dll
```

UE4SS.def declares only the required host imports. The DLL exports
`start_mod` and `uninstall_mod`. It does not bundle UE4SS itself.

The population interception contract checks the complete builder, row-name,
row-validity and caller instruction ranges, plus the reflected layouts and
frame-counter signature used by the helper. The appearance repair also checks
the humanoid initializer, animal coat writer and humanoid appearance reader,
plus the AI stub and humanoid definition layouts. A changed required contract
disables replacement while retaining Lua population adjustment. These checks
do not impose a whole-file hash, version or storefront restriction.

Replacement entries clear their copied animal activity montage arrays before
the engine generates action points. This uses reflected array allocation,
copy, comparison and destruction, with the original array backed up for
rollback. Source entries are retained. The empty array selects the engine's
montage-free roaming branch; enemy quantities and spawn points are unchanged.
The contract checks this branch and its dispatch/preparation paths, the montage
element type and the dynamic point layouts. Already generated activity points
prevent conversion of that encounter.

`AppearanceRepair.hpp` handles the saved field shared by animal coat variants
and human appearance IDs. The native initializer hook is restricted to the
normal bandit definition and the original supported wildlife row names. It
clears a nonzero animal value only while the human appearance row is unset,
then invokes normal game initialization once. Initialized human appearances,
ordinary bandits, shared assets, inventory and unrelated save fields are retained.
The hook adds no polling or world scans.

Wildlife definition comparisons use package and asset FNames cached at binding,
with numeric suffixes retained. Encounter validation finishes before allocating
or importing replacement values. Engine property operations still construct,
copy and destroy soft references; the cache contains no UObject pointers or
borrowed property values. The per-area and per-frame limits remain unchanged.

The prototype exposes independent boar-to-bandit and wolf-to-bandit replacement at 0 or 100 only. `WildlifeDefinitions.hpp` lists exact supported stock definitions, including brown, white and astral wolves. The Lua population matcher uses the same definitions.
`ReplacementPolicy.hpp` contains the planned percentage/outcome selection
logic; it is not connected to respawn-cycle persistence yet. Other enemy
outcomes and suppression are not enabled in this prototype.

`EncounterCycle.hpp` implements the engine-independent decision lifecycle and
a 40-byte versioned record format. Restored records keep their outcome despite
settings changes. Partial refills and forced respawns cannot start a new cycle;
natural advancement requires confirmed completion and eligibility with no
remaining members. No spawn stays pending until the game acknowledges actual
suppression. Invalid identities, records and random draws are rejected.

This decision layer is not connected to the game save or respawn services.
The adapter must supply verified lifecycle events and persist each record in
the matching game save before applying it. A successful respawn attempt alone
does not establish a new cycle. Definition codes in record version 1 refer to
the current ten-entry stock list; reordering that list requires migration.

`RespawnObserver.inl` adds optional observations of death scheduling,
eligibility, respawn attempts and clock advancement. Its four hooks preserve
all arguments and original return values, call each original exactly once,
and retain only owned snapshots across callbacks. It never changes queues,
time, encounters or saved decisions. A successful partial refill or forced
attempt is not treated as a new natural cycle.

`RespawnContract.hpp` validates the observation functions and the scheduling,
visibility, cleanup and time-dispatch paths they depend on. Reflected service
layout checks and indexed type identity/deletion checks also apply. Failure
removes only the optional hooks; the required replacement contract remains
independent. The observed next-day flag follows a forward change in the game
clock's day number. Queued entries still need the population visibility check
and dead-stub cleanup before normal refill processing.

Logging Off bypasses observation reads, timing and formatting. Logging On
uses a 128-entry session-only cache, at most one eligibility sample per group
per second, and a shared ceiling of 256 eligibility samples or a soft 2 ms of
capture work per second. Member snapshots are capped at 128. Group detail
reports are limited to 12 per ten seconds, with at most one clock summary and
one aggregate report per ten seconds of activity. These are work bounds,
not measured game frame-time results.
