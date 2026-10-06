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

## Runtime integration

The native helper has seven required hooks: generated population-table construction,
humanoid appearance initialization, activity binding for suppression, death scheduling,
respawn eligibility, attempt dispatch and game-clock advancement. Required-hook failure
removes the partial installation and leaves Lua population adjustment available.
`_LWConfigureReplacementV1` accepts boar chance, wolf chance, boar pool bitmask,
wolf pool bitmask and Logging. Lua consumes these in order; each pool has six bits. Existing bit positions and saved outcome IDs stay fixed;
Regular guards use the appended bit 5 / outcome ID 6, after No spawn.

`ReplacementPolicy.hpp` selects once per whole group. `EncounterCycle.hpp` defines
the cycle rules and a versioned 40-byte record keyed by original area GUID, source
row and wildlife definition. The ten definition codes follow `WildlifeDefinitions.hpp`;
reordering them requires migration. Original outcomes are saved too, so changing
settings or revisiting an area does not generate extra rolls.

`DecisionJournal.hpp` persists records in the matching game save's integer FactsDB.
Two reserved banks, full-identity decoding, checksums, collision probes and a final
head switch protect against partial updates and occupied names. An invalid committed
record fails validation. No global sidecar chooses outcomes for unrelated saves.
`EncounterRuntime.inl` obtains FactsDB through reflected game-instance APIs with checked
function pointers, fields and parameter layouts. Quest types belong to `/Script/Quest`.
Fact methods are resolved by name on the verified FactsDB default object, avoiding
function-path punctuation assumptions. Missing bindings report their exact identity.
This adapter performs no direct save-file IO.

Generated row names, group quantities, locations and respawn policies stay intact.
Definitions and row AI overrides use engine FProperty allocation, import, copy,
comparison and destruction. Regular guards and blood guards receive the hostile human reactions and
global-bandit faction; other outcomes use their stock profiles. Foreign overrides,
quest start conditions and scripted/fixed encounters are excluded.

`RespawnObserver.inl` also drives saved lifecycle decisions. Completion requires the
registered dead-member callback with no living members. Only an eligible, unforced
natural attempt with no remaining members advances the cycle. Travel, restoration,
partial refills and cleanup attempts retain it. Callback entry/stub pointers are
borrowed only for their synchronous call; cross-callback records contain owned values.

No spawn intercepts the normal activity-binding boundary after registration and
before actor creation. It verifies the encounter membership, population registry,
saved record, absent actor and absent activity binding, then calls normal completion
bookkeeping. The original binding function runs once and rejects the completed stub.
The game retains its dead-member persistence and next-day queue. No zero quantities,
live actor destruction, artificial clock advancement or forced respawning are used.

Replacement rows clear copied animal montage arrays before point generation. For
later enemy cycles, existing points belonging to that group clear their montage
through the normal point setter, with backup/readback/rollback. Locations and other
groups' points remain intact. `AppearanceRepair.hpp` handles the field shared by
animal coats and human appearance IDs. Normal restoration retains a human choice;
new cycles reset reused fields before normal randomized humanoid initialization.

`EquipmentRepair.inl` runs before that same initializer. For a living, actorless
saved bandit, regular guard or blood guard, it checks the stock weapon slot against the saved
active loadout. A missing weapon requests the game's normal equipment seeding;
the helper does not write inventory contents. Separate equipment journal records
include the persistent member ID and mark initialization once per encounter cycle,
including already armed members. Subsequent loads preserve disarming and looting.
These records use their own namespace and the reserved word in the checked record;
the original encounter journal format and namespace remain unchanged.

`NativeContract.hpp`, `RespawnContract.hpp` and `LifecycleContract.hpp` check the
instruction ranges, call sites and layout-dependent functions actually used.
Additional reflected field, class, property and native-function checks run at binding.
These are capability checks, not game-version, storefront or whole-file hash gates.
No FWeakObjectPtr constructor or serial allocation is used. Retained area identities
use indexed slots, existing serials and deletion notification, with a 4096-entry
limit and eight-slot pruning. Tables are bounded to 64 rows, member lists to 128,
and generated point lists to 4096. Builder work has a 128-entry and soft 2 ms frame
budget; deferred tables retain their current values and may be revisited normally.

Logging Off skips optional diagnostics and timing, while required saved-decision
processing stays active. Logging On retains the bounded 128-key observation cache,
one eligibility sample per group per second, and at most 256 samples or soft 2 ms
of capture per second. Details are capped at 12 per ten seconds with aggregated
counts/timings. These are work bounds, not measured frame-time results.
