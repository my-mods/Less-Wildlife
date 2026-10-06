// Included inside LessWildlife after the common native helpers. These
// diagnostics accompany the saved encounter lifecycle in EncounterRuntime.inl.
namespace RespawnObserver {
using Queue = void(*)(UObject*, UObject*);
using Eligibility = bool(*)(UObject*, void*);
using Attempt = bool(*)(UObject*, void*, void*, bool);
using ClockAdvance = void(*)(UObject*, int64_t, bool, bool);
Queue originalQueue{};
Eligibility originalEligibility{};
Attempt originalAttempt{};
ClockAdvance originalClock{};
std::array<void*, 4> targets{};
RespawnLogCache cache;
uint64_t window{}, reports{}, events{}, sampled{}, dropped{}, micros{}, maximum{};
uint64_t sampleWindow{}, sampleMicros{}, sampleAttempts{}, limitedSamples{}, lastClockReport{};
std::atomic_bool enabled{};

void observationFailure() noexcept {
    try { warning(L"Respawn observation unavailable for an entry; normal game processing retained."); }
    catch (...) { /* Diagnostics must not interrupt the original operation. */ }
}

bool observing(UObject* service) {
    return logging && active && GetCurrentThreadId() == gameThread && enabled
        && service && observerType.valid() && service->IsA(static_cast<UClass*>(observerType.object));
}
bool capture(UObject* service, void* entry, RespawnEvent event, RespawnSnapshot& sample) {
    if (!observing(service) || !entry) return false;
    ++events;
    const auto row = read<void*>(entry, 0x10);
    if (!row) return false;
    auto definition = definitionKey(static_cast<unsigned char*>(row) + 0x30);
    if (definitionSpecies(definition) == Species::None && std::find(enemyKeys.begin(), enemyKeys.end(), definition) == enemyKeys.end()) return false;
    sample.area = read<std::array<uint32_t, 4>>(entry, 0x110);
    sample.row = read<uint64_t>(entry, 8);
    if (!(sample.area[0] || sample.area[1] || sample.area[2] || sample.area[3])) return false;
    const auto now = GetTickCount64();
    if (!cache.reserve(sample, event, now)) return false;
    // This conversion runs at most once per second per eligibility key, and
    // only after rejecting all non-bandit rows without string construction.
    if (!wildlifeRow(read<FName>(entry, 8).ToString())) return false;
    sample.phase = read<int16_t>(entry, 0x66);
    const auto count = read<int32_t>(entry, 0x88), capacity = read<int32_t>(entry, 0x8c);
    require(count >= 0 && count <= 128 && capacity >= count && capacity <= 65536, "respawn member array layout");
    auto data = read<unsigned char*>(entry, 0x80);
    require(data || count <= 1, "respawn inline member capacity");
    if (!data) data = static_cast<unsigned char*>(entry) + 0x78;
    for (int i = 0; i < count; ++i) {
        auto stub = read<UObject*>(data, i * sizeof(void*));
        require(stub && stub->IsA(stubType), "respawn member type");
        if (read<uint8_t>(stub, 0x58) & 0x10) ++sample.dead; else ++sample.living;
    }
    sample.recycled = read<int32_t>(entry, 0xd0);
    require(sample.recycled >= 0 && sample.recycled <= 65536, "respawn recycled ID count");
    ++sampled;
    return true;
}
void report(const RespawnSnapshot& sample, RespawnEvent event, RespawnEvidence evidence) {
    if (!logging || !cache.changed(sample, event, evidence)) return;
    const auto now = GetTickCount64();
    if (now - window >= 10000) {
        window = now; reports = 0;
        message(L"Respawn observations=" + std::to_wstring(events) + L", samples=" + std::to_wstring(sampled)
            + L", limited reports=" + std::to_wstring(dropped) + L", capture us=" + std::to_wstring(micros)
            + L", max capture us=" + std::to_wstring(maximum) + L", limited samples=" + std::to_wstring(limitedSamples));
    }
    if (reports++ >= 12) { ++dropped; return; }
    constexpr const wchar_t* labels[] = {L"death queued for next day", L"eligible for an attempt",
        L"waiting: dead member still inside population visibility region", L"waiting: population system unavailable",
        L"waiting: inactive phase", L"dead stubs cleaned; another attempt required", L"attempt deferred",
        L"forced attempt accepted (not a natural cycle)", L"attempt accepted with surviving members (not a new pack)",
        L"natural attempt accepted with no surviving members (verify spawned group)"};
    constexpr wchar_t digits[] = L"0123456789abcdef";
    std::wstring guid; guid.reserve(32);
    for (auto word : sample.area) for (unsigned byte = 0; byte < 4; ++byte) {
        const auto value = (word >> (byte * 8)) & 255;
        guid += digits[value >> 4]; guid += digits[value & 15];
    }
    const auto row = read<FName>(&sample.row, 0).ToString();
    message(L"Respawn: " + std::wstring(labels[static_cast<unsigned>(evidence)]) + L"; area=" + guid + L", row=" + row
        + L", living=" + std::to_wstring(sample.living) + L", dead=" + std::to_wstring(sample.dead)
        + L", recycled IDs=" + std::to_wstring(sample.recycled));
}
void reportSafely(const RespawnSnapshot& sample, RespawnEvent event, RespawnEvidence evidence) noexcept {
    try { report(sample, event, evidence); }
    catch (...) { observationFailure(); }
}
bool measure(UObject* service, void* entry, RespawnEvent event, RespawnSnapshot& sample) {
    if (!observing(service)) return false;
    const auto now = GetTickCount64();
    if (now - sampleWindow >= 1000) { sampleWindow = now; sampleAttempts = sampleMicros = 0; }
    if (event == RespawnEvent::Eligibility && (sampleAttempts >= 256 || sampleMicros >= 2000)) { ++limitedSamples; return false; }
    ++sampleAttempts;
    const auto started = std::chrono::steady_clock::now();
    bool result{};
    try { result = capture(service, entry, event, sample); }
    catch (...) { observationFailure(); }
    const auto us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
    micros += us; sampleMicros += us; maximum = std::max(maximum, us);
    return result;
}
void queued(UObject* service, UObject* stub) {
    HookTimer timing(3);
    RespawnSnapshot sample{}; bool observed{};
    if (observing(service) && stub && stub->IsA(stubType)) {
        auto row = read<void*>(stub, 0x60);
        if (row && read<uint8_t>(row, 0xc0) == 1)
            observed = measure(service, read<void*>(stub, 0xe8), RespawnEvent::Queued, sample);
    }
    try { EncounterRuntime::queued(service, stub); }
    catch (...) { if (logging) warning(L"Encounter completion could not be saved."); }
    originalQueue(service, stub);
    if (observed) reportSafely(sample, RespawnEvent::Queued, RespawnEvidence::NextDayQueued);
}
bool eligible(UObject* service, void* entry) {
    HookTimer timing(4);
    RespawnSnapshot sample{};
    const bool observed = measure(service, entry, RespawnEvent::Eligibility, sample);
    const bool populationAvailable = observed && read<void*>(service, 0x48);
    const bool result = originalEligibility(service, entry);
    if (observed) reportSafely(sample, RespawnEvent::Eligibility, result ? RespawnEvidence::Eligible
        : populationAvailable ? RespawnEvidence::VisibilityBlocked : RespawnEvidence::PopulationUnavailable);
    return result;
}
bool attempt(UObject* service, void* entry, void* context, bool forced) {
    HookTimer timing(5);
    RespawnSnapshot sample{};
    const bool observed = measure(service, entry, RespawnEvent::Attempt, sample);
    EncounterRuntime::AttemptScope scope;
    if (active && configurationReady && !failed && GetCurrentThreadId() == gameThread && enabled) {
        try { EncounterRuntime::prepareAttempt(service, entry, forced, !forced && context == nullptr, scope); }
        catch (const std::exception& error) {
            if (logging && warningCount.fetch_add(1) < 8) {
                std::string text(error.what()); message(L"Saved encounter unavailable: " + std::wstring(text.begin(), text.end()));
            }
        }
    }
    struct ScopeGuard {
        EncounterRuntime::AttemptScope* previous;
        explicit ScopeGuard(EncounterRuntime::AttemptScope* current) : previous(EncounterRuntime::attemptScope) { EncounterRuntime::attemptScope = current; }
        ~ScopeGuard() { EncounterRuntime::attemptScope = previous; }
    } guard(&scope);
    const bool result = originalAttempt(service, entry, context, forced);
    if (result && scope.decision) EncounterRuntime::consumedFresh(scope.decision->key);
    if (logging && scope.suppressed && reports++ < 12) message(L"No spawn: completed " + std::to_wstring(scope.suppressed) + L" registered stubs; original respawn policy retained.");
    // Never read the borrowed entry/stub array after spawning callbacks run.
    if (observed) reportSafely(sample, RespawnEvent::Attempt, classifyAttempt(sample, forced, result));
    return result;
}
void clockAdvance(UObject* service, int64_t hours, bool nextDay, bool contextFlag) {
    HookTimer timing(6);
    const bool observed = observing(service);
    const int pending = observed ? read<int32_t>(service, 0xe0) : 0;
    originalClock(service, hours, nextDay, contextFlag);
    if (observed && logging && (nextDay || hours) && pending > 0 && GetTickCount64() - lastClockReport >= 10000) {
        lastClockReport = GetTickCount64();
        try {
            message(L"Respawn clock: elapsed hours=" + std::to_wstring(hours) + L", next day=" + (nextDay ? L"yes" : L"no")
                + L", groups awaiting next day=" + std::to_wstring(pending));
        } catch (...) { observationFailure(); }
    }
}
void stop() {
    enabled = false;
    for (auto& target : targets) if (target) { MH_DisableHook(target); MH_RemoveHook(target); target = nullptr; }
    observerType.alive = false; cache.clear();
}
void start() {
    try {
        observerType.bind(UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, L"/Script/Population.CommunityRespawnServiceImpl"));
        auto type = static_cast<UClass*>(observerType.object);
        require(type->GetPropertiesSize() == 0x100, "respawn service size changed");
        field(type, L"StubSystem", 0x40, 8, L"ObjectProperty");
        field(type, L"PopulationSystem", 0x48, 8, L"ObjectProperty");
        std::wstring error;
        if (!RespawnContract::validate(error)) {
            if (logging) message(L"Encounter lifecycle unavailable: " + error + L". Replacement cannot start; population controls remain available.");
            stop(); return;
        }
        const std::array<void*, 4> callbacks{reinterpret_cast<void*>(&queued), reinterpret_cast<void*>(&eligible),
            reinterpret_cast<void*>(&attempt), reinterpret_cast<void*>(&clockAdvance)};
        const std::array<void**, 4> originals{reinterpret_cast<void**>(&originalQueue), reinterpret_cast<void**>(&originalEligibility),
            reinterpret_cast<void**>(&originalAttempt), reinterpret_cast<void**>(&originalClock)};
        for (size_t i = 0; i < targets.size(); ++i) {
            auto candidate = RespawnContract::address(i);
            require(MH_CreateHook(candidate, callbacks[i], originals[i]) == MH_OK, "respawn observation hook creation failed");
            targets[i] = candidate;
        }
        for (auto target : targets) require(MH_EnableHook(target) == MH_OK, "respawn observation hook activation failed");
        enabled = true;
        if (logging) message(L"Encounter lifecycle hooks ready; Logging records scheduling, visibility, cleanup and refill evidence.");
    } catch (const std::exception& error) {
        stop();
        if (logging) try {
            std::string detail(error.what()); message(L"Encounter lifecycle unavailable: " + std::wstring(detail.begin(), detail.end())
                + L". Replacement cannot start; population controls remain available.");
        } catch (...) { /* Startup still checks enabled when diagnostics fail. */ }
    }
}
}
