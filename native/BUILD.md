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
