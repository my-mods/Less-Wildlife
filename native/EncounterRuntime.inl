// Included after the reflected property helpers. All engine access is confined
// to the game thread and synchronous population callbacks.
namespace EncounterRuntime {
using BindActivity = void(*)(UObject*, int16_t, void*, void*);
BindActivity originalBind{};
void* bindTarget{};
using CompleteStub = void(*)(UObject*);
std::mt19937 random;
FProperty* reactionsProperty{};
FProperty* factionProperty{};
constexpr wchar_t hostileReactions[] = L"/Game/_Dawnwalker/NPC/BasicNPC/Reactions/AIReactions_Human_Hostile.AIReactions_Human_Hostile_C";
FName hostileFaction;
struct GuidHash {
    size_t operator()(const std::array<uint32_t, 4>& value) const noexcept {
        size_t h = 2166136261u; for (auto word : value) { h ^= word; h *= 16777619u; } return h;
    }
};
struct AreaHandle { TypeRef object; std::array<uint32_t, 4> guid; std::array<uint64_t, 10> fresh{}; };
std::mutex areaMutex;
std::vector<std::shared_ptr<AreaHandle>> areaSlots;
std::unordered_map<std::array<uint32_t, 4>, size_t, GuidHash> areaByGuid;
std::unordered_map<int32_t, size_t> areaByIndex;
size_t pruneArea{};
void invalidateArea(int32_t index) {
    std::lock_guard guard(areaMutex);
    if (auto found = areaByIndex.find(index); found != areaByIndex.end()) areaSlots[found->second]->object.alive = false;
}
void rememberArea(UObject* area) {
    auto item = std::make_shared<AreaHandle>(); item->object.bind(area);
    item->guid = read<std::array<uint32_t, 4>>(area, 0x1e0);
    std::lock_guard guard(areaMutex);
    size_t slot = areaSlots.size();
    if (auto found = areaByGuid.find(item->guid); found != areaByGuid.end()) {
        slot = found->second;
        if (areaSlots[slot]->object.alive && areaSlots[slot]->object.object == area) return;
    } else if (slot == 4096) {
        for (unsigned i = 0; i < 8; ++i) {
            auto candidate = pruneArea++ % areaSlots.size();
            if (!areaSlots[candidate]->object.alive) { slot = candidate; break; }
        }
        require(slot != 4096, "loaded encounter area cache limit");
    }
    if (slot == areaSlots.size()) areaSlots.push_back(item);
    else {
        areaByGuid.erase(areaSlots[slot]->guid); areaByIndex.erase(areaSlots[slot]->object.index);
        areaSlots[slot] = item;
    }
    areaByGuid[item->guid] = slot; areaByIndex[item->object.index] = slot;
}
std::shared_ptr<AreaHandle> findArea(const EncounterKey& key) {
    std::lock_guard guard(areaMutex);
    auto found = areaByGuid.find(key.area);
    return found == areaByGuid.end() ? nullptr : areaSlots[found->second];
}

template<class T> T engine(uint32_t rva) {
    return reinterpret_cast<T>(reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)) + rva);
}
unsigned uniform(unsigned bound) { return std::uniform_int_distribution<unsigned>(0, bound - 1)(random); }
void configure(int64_t boars, int64_t wolves, int64_t boarPool, int64_t wolfPool) {
    if (boars < 0 || boars > 100 || wolves < 0 || wolves > 100 || boarPool < 0 || boarPool > 31 || wolfPool < 0 || wolfPool > 31) {
        replacementOptions = 0; replacementMask = 0; configurationReady = false; return;
    }
    replacementOptions = uint64_t(boars) | (uint64_t(wolves) << 8) | (uint64_t(boarPool) << 16) | (uint64_t(wolfPool) << 24);
    replacementMask = (boars && boarPool ? 1 : 0) | (wolves && wolfPool ? 2 : 0);
    configurationReady = true;
}
Options options(uint8_t definition) {
    const auto settings = replacementOptions.load();
    const bool wolf = wildlifeDefinitions.at(definition).species == Species::Wolf;
    Options result; result.chance = (settings >> (wolf ? 8 : 0)) & 255;
    const auto mask = (settings >> (wolf ? 24 : 16)) & 31;
    for (unsigned i = 0; i < 5; ++i) result.allowed[i] = (mask & (uint64_t{1} << i)) != 0;
    return result;
}
bool enemyDefinition(UObject* definition) {
    const auto name = definition->GetClassPrivate()->GetFName();
    for (size_t i = 0; i < enemyKeys.size(); ++i)
        if (name == enemyKeys[i].asset) return definition->GetClassPrivate()->GetPathName() == enemies[i];
    return false;
}
const wchar_t* path(const EncounterDecision& decision) {
    const auto outcome = static_cast<unsigned>(decision.outcome);
    return outcome >= 1 && outcome <= 4 ? enemies[outcome - 1] : wildlifeDefinitions[decision.key.definition].path.data();
}
DefinitionKey keyFor(const EncounterDecision& decision) {
    const auto outcome = static_cast<unsigned>(decision.outcome);
    return outcome >= 1 && outcome <= 4 ? enemyKeys[outcome - 1] : wildlifeKeys[decision.key.definition];
}
std::optional<EncounterKey> identify(const std::array<uint32_t, 4>& area, FName row) {
    auto name = row.ToString(); auto separator = name.find(L'_');
    if (separator == std::wstring::npos || !separator || separator > 2) return {};
    unsigned index{};
    for (size_t i = 0; i < separator; ++i) {
        if (name[i] < L'0' || name[i] > L'9') return {};
        index = index * 10 + name[i] - L'0';
    }
    for (uint8_t d = 0; d < wildlifeKeys.size(); ++d) {
        if (name.substr(separator + 1) != wildlifeKeys[d].asset.ToString()) continue;
        EncounterKey result{area, static_cast<uint8_t>(index), d};
        if (index < 64 && valid(result)) return result;
    }
    return {};
}
void noteFresh(const EncounterKey& key) {
    if (auto area = findArea(key)) {
        const auto index = key.row * 10u + key.definition;
        area->fresh[index / 64] |= uint64_t{1} << (index % 64);
    }
}
bool isFresh(const EncounterKey& key) {
    if (auto area = findArea(key)) {
        const auto index = key.row * 10u + key.definition;
        return (area->fresh[index / 64] & (uint64_t{1} << (index % 64))) != 0;
    }
    return false;
}
void consumedFresh(const EncounterKey& key) {
    if (auto area = findArea(key)) {
        const auto index = key.row * 10u + key.definition;
        area->fresh[index / 64] &= ~(uint64_t{1} << (index % 64));
    }
}
void bind() {
    auto find = [](const wchar_t* name) { return UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, name); };
    types[8].bind(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"));
    types[9].bind(types[8].object->GetFunctionByNameInChain(L"GetGameInstanceSubsystem"));
    types[10].bind(find(L"/Script/QuestSystem.QuestSystemImpl"));
    types[11].bind(find(L"/Script/QuestSystem.Default__QuestSystemBlueprintLibrary"));
    types[12].bind(types[11].object->GetFunctionByNameInChain(L"GetFactsDB"));
    types[13].bind(find(L"/Script/FactsDB.FactsDB"));
    types[14].bind(find(L"/Script/FactsDB.FactsDB:FactDoesExist"));
    types[15].bind(find(L"/Script/FactsDB.FactsDB:FactGetInt"));
    types[16].bind(find(L"/Script/FactsDB.FactsDB:FactSetInt"));
    types[17].bind(find(L"/Script/Population.PopulationSystemImpl"));
    types[18].bind(find(L"/Script/Population.DynamicActionPoint"));
    require(static_cast<UClass*>(types[18].object)->GetPropertiesSize() == 0x440, "dynamic activity point layout changed");
    auto function = [](unsigned index, int size) {
        auto f = static_cast<UFunction*>(types[index].object);
        require(f->GetParmsSize() == size, "saved decision function parameters changed"); return f;
    };
    auto f = function(9, 24);
    field(f, L"ContextObject", 0, 8, L"ObjectProperty");
    field(f, L"Class", 8, 8, L"ClassProperty");
    field(f, L"ReturnValue", 16, 8, L"ObjectProperty");
    f = function(12, 16);
    field(f, L"QuestSystemInterface", 0, 8, L"ObjectProperty");
    field(f, L"ReturnValue", 8, 8, L"ObjectProperty");
    for (unsigned i = 14; i <= 16; ++i) {
        f = function(i, i == 14 ? 9 : 12);
        field(f, L"FactName", 0, 8, L"StructProperty");
        field(f, i == 16 ? L"Value" : L"ReturnValue", 8, i == 14 ? 1 : 4, i == 14 ? L"BoolProperty" : L"IntProperty");
    }
    auto facts = static_cast<UClass*>(types[13].object);
    require(facts->GetPropertiesSize() == 0xd0, "saved facts layout changed");
    field(facts, L"FactsDelegates", 0x80, 0x50, L"MapProperty");
    auto population = static_cast<UClass*>(types[17].object);
    require(population->GetPropertiesSize() == 0x9b0, "population registry layout changed");
    field(population, L"Registry", 0x350, 0x50, L"MapProperty");
    reactionsProperty = field(rowType, L"AIReactions", 0x60, 8, L"ClassProperty");
    factionProperty = field(rowType, L"AIFaction", 0x68, 8, L"StructProperty");
    hostileFaction = FName(L"RebelAI.Faction.GlobalBandit");
    for (auto [index, rva] : std::array<std::pair<unsigned, uint32_t>, 5>{{{9, 0x1b77bcc}, {12, 0x2326a00},
            {14, 0x5792908}, {15, 0x251880c}, {16, 0x5792a00}}})
        require(reinterpret_cast<void*>(static_cast<UFunction*>(types[index].object)->GetFuncPtr()) == engine<void*>(rva),
            "saved decision function binding changed");
    std::wstring error;
    require(LifecycleContract::validate(error), "saved decision / suppression code contract changed");
    std::random_device source; std::seed_seq seed{source(), source(), source(), source()}; random.seed(seed);
}

struct Facts {
    TypeRef database;
    explicit Facts(UObject* context) {
        for (unsigned i = 8; i < types.size(); ++i) require(types[i].valid(), "saved decision metadata expired");
        struct Subsystem { UObject* context; UObject* type; UObject* result; } subsystem{context, types[10].object, nullptr};
        types[8].object->ProcessEvent(static_cast<UFunction*>(types[9].object), &subsystem);
        require(subsystem.result && subsystem.result->IsA(static_cast<UClass*>(types[10].object)), "quest save subsystem unavailable");
        struct Database { UObject* quest; UObject* result; } parameters{subsystem.result, nullptr};
        types[11].object->ProcessEvent(static_cast<UFunction*>(types[12].object), &parameters);
        require(parameters.result && parameters.result->IsA(static_cast<UClass*>(types[13].object)), "saved facts database unavailable");
        database.bind(parameters.result);
    }
    void verifyDatabase() { require(database.valid(), "saved decision database expired"); }
    std::optional<int32_t> get(const std::wstring& name) {
        verifyDatabase();
        struct Exists { FName name; bool result{}; } exists{FName(name.c_str())};
        database.object->ProcessEvent(static_cast<UFunction*>(types[14].object), &exists);
        if (!exists.result) return {};
        struct Get { FName name; int32_t result{}; } value{exists.name};
        database.object->ProcessEvent(static_cast<UFunction*>(types[15].object), &value);
        return value.result;
    }
    void set(const std::wstring& name, int32_t value) {
        verifyDatabase(); struct Set { FName name; int32_t value; } parameters{FName(name.c_str()), value};
        database.object->ProcessEvent(static_cast<UFunction*>(types[16].object), &parameters);
    }
    bool distinct(const std::wstring& root, unsigned count) {
        std::array<uint32_t, 21> hashes{}; require(count <= hashes.size(), "journal key limit");
        for (unsigned i = 0; i < count; ++i) {
            FName name((root + L".w" + std::to_wstring(i)).c_str());
            hashes[i] = engine<uint32_t(*)(void*, const FName*)>(0x13de99c)(database.object, &name);
            for (unsigned j = 0; j < i; ++j) if (hashes[i] == hashes[j]) return false;
        }
        return true;
    }
};

void setDefinition(void* row, const EncounterDecision& decision, UObject* owner) {
    auto destination = static_cast<unsigned char*>(row) + 0x30;
    auto reactions = static_cast<unsigned char*>(row) + 0x60;
    auto faction = static_cast<unsigned char*>(row) + 0x68;
    const auto current = definitionKey(destination);
    auto priorReaction = read<UClass*>(reactions, 0);
    require((current == wildlifeKeys[decision.key.definition] || std::find(enemyKeys.begin(), enemyKeys.end(), current) != enemyKeys.end())
        && read<uint64_t>(row, 0x58) == 0 && emptyTags(static_cast<unsigned char*>(row) + 0x70)
        && (!priorReaction || priorReaction->GetPathName() == hostileReactions)
        && (!read<uint64_t>(faction, 0) || read<FName>(faction, 0) == hostileFaction), "encounter profile changed externally");
    if (definitionKey(destination) == keyFor(decision)) {
        auto existing = read<UClass*>(reactions, 0);
        if (decision.outcome == Outcome::BloodGuard
            ? existing && existing->GetPathName() == hostileReactions && read<FName>(faction, 0) == hostileFaction
            : !existing && read<uint64_t>(faction, 0) == 0) return;
    }
    Value replacement(definitionProperty), backup(definitionProperty), nextReactions(reactionsProperty), nextFaction(factionProperty),
        previousReactions(reactionsProperty), previousFaction(factionProperty);
    auto end = definitionProperty->ImportText_Direct(path(decision), replacement.data, owner, 0, nullptr);
    require(end && !*end && definitionKey(replacement.data) == keyFor(decision), "enemy soft reference import failed");
    if (decision.outcome == Outcome::BloodGuard) {
        // The stock guard uses loyal-neutral reactions and a soldier faction.
        // These roaming replacements use the stock hostile human reactions and
        // outlaw faction; their own definition still supplies guard combat AI,
        // equipment, abilities, appearance and loot.
        end = reactionsProperty->ImportText_Direct(hostileReactions, nextReactions.data, owner, 0, nullptr);
        auto type = read<UClass*>(nextReactions.data, 0);
        require(end && !*end && type && type->GetPathName() == hostileReactions, "hostile guard reactions unavailable");
        end = factionProperty->ImportText_Direct(L"(TagName=\"RebelAI.Faction.GlobalBandit\")", nextFaction.data, owner, 0, nullptr);
        require(end && !*end && read<FName>(nextFaction.data, 0) == hostileFaction, "hostile guard faction unavailable");
    }
    definitionProperty->CopyCompleteValue(backup.data, destination);
    reactionsProperty->CopyCompleteValue(previousReactions.data, reactions);
    factionProperty->CopyCompleteValue(previousFaction.data, faction);
    try {
        definitionProperty->CopyCompleteValue(destination, replacement.data);
        reactionsProperty->CopyCompleteValue(reactions, nextReactions.data);
        factionProperty->CopyCompleteValue(faction, nextFaction.data);
        require(definitionKey(destination) == keyFor(decision) && reactionsProperty->Identical(reactions, nextReactions.data)
            && factionProperty->Identical(faction, nextFaction.data), "enemy profile readback failed");
    } catch (...) {
        definitionProperty->CopyCompleteValue(destination, backup.data);
        reactionsProperty->CopyCompleteValue(reactions, previousReactions.data);
        factionProperty->CopyCompleteValue(faction, previousFaction.data);
        require(definitionKey(destination) == definitionKey(backup.data) && reactionsProperty->Identical(reactions, previousReactions.data)
            && factionProperty->Identical(faction, previousFaction.data), "enemy profile rollback failed");
        throw;
    }
}
void prepareRoamingPoints(const EncounterDecision& decision) {
    if (decision.outcome == Outcome::Original || decision.outcome == Outcome::None) return;
    auto handle = findArea(decision.key);
    // A streamed-out area has no surviving generated points. On its next
    // registration the builder supplies the montage-free copied entry.
    if (!handle || !handle->object.valid()) return;
    auto area = handle->object.object;
    require(area->IsA(areaType) && read<std::array<uint32_t, 4>>(area, 0x1e0) == decision.key.area, "respawn area identity changed");
    require(read<Array>(area, 0x350).count == 0, "respawn area is quest-gated");
    auto table = read<UDataTable*>(area, 0x370); require(table && table->GetRowStruct() == rowType, "respawn area table unavailable");
    auto& rows = table->GetRowMap(); require(rows.Num() <= 64 && rows.GetMaxIndex() <= 64, "respawn row limit");
    uint32_t ordinal{}; bool found{};
    for (auto& row : rows) {
        if (identify(decision.key.area, row.Key) == std::optional{decision.key}) { found = true; break; }
        ++ordinal;
    }
    require(found, "respawn source row missing");
    const uint32_t group = read<uint32_t>(area, 0x414) * 1000u + ordinal + 1;
    auto points = read<Array>(area, 0x398); require(arrayValid(points, 4096), "dynamic activity point limit");
    auto property = montageProperty->GetInner(); Value empty(property);
    struct Point { UObject* object; std::unique_ptr<Value> backup; };
    std::vector<Point> changes;
    for (int i = 0; i < points.count; ++i) {
        auto point = read<UObject*>(points.data, i * 8);
        require(point && point->IsA(static_cast<UClass*>(types[18].object)), "dynamic activity point class");
        if (read<uint32_t>(point, 0x438) != group) continue;
        auto parameters = read<unsigned char*>(point, 0x410);
        require(parameters && read<uint64_t>(parameters, 0x280) == read<uint64_t>(engine<void*>(0xa06c7f8), 0), "dynamic activity parameter type");
        if (property->Identical(parameters + 0x288, empty.data)) continue;
        auto backup = std::make_unique<Value>(property);
        property->CopyCompleteValue(backup->data, parameters + 0x288);
        changes.push_back({point, std::move(backup)});
    }
    size_t changed{};
    try {
        for (auto& item : changes) {
            ++changed;
            engine<void(*)(UObject*, void*, const void*)>(0x1e95b04)(item.object, nullptr, empty.data);
            require(property->Identical(read<unsigned char*>(item.object, 0x410) + 0x288, empty.data), "roaming point readback failed");
        }
    } catch (...) {
        while (changed) {
            auto& item = changes[--changed];
            engine<void(*)(UObject*, void*, const void*)>(0x1e95b04)(item.object, nullptr, item.backup->data);
            require(property->Identical(read<unsigned char*>(item.object, 0x410) + 0x288, item.backup->data), "roaming point rollback failed");
        }
        throw;
    }
}
struct Members { unsigned count{}, living{}; bool contains{}; };
Members members(void* entry, UObject* sought = nullptr) {
    const auto count = read<int32_t>(entry, 0x88), capacity = read<int32_t>(entry, 0x8c);
    require(count >= 0 && count <= 128 && capacity >= count && capacity <= 65536, "encounter member limit");
    auto data = read<unsigned char*>(entry, 0x80);
    require(data || count <= 1, "encounter member storage");
    if (!data) data = static_cast<unsigned char*>(entry) + 0x78;
    Members result{static_cast<unsigned>(count)};
    for (int i = 0; i < count; ++i) {
        auto stub = read<UObject*>(data, i * 8);
        require(stub && stub->IsA(stubType), "encounter member class");
        if (!(read<uint8_t>(stub, 0x58) & 0x10)) ++result.living;
        if (stub == sought) result.contains = true;
    }
    return result;
}
std::optional<EncounterKey> entryKey(void* entry) {
    if (!entry) return {};
    auto row = read<unsigned char*>(entry, 0x10);
    if (!row || read<uint8_t>(row, 0x90) != 3 || read<uint8_t>(row, 0xc0) != 1) return {};
    return identify(read<std::array<uint32_t, 4>>(entry, 0x110), read<FName>(entry, 8));
}
struct AttemptScope {
    // entry is borrowed only while the game's synchronous attempt is on-stack.
    // Completion and diagnostic reporting use owned values, never this pointer.
    void* entry{}; std::optional<EncounterDecision> decision;
    unsigned suppressed{}; bool suppressionFailed{}, newCycle{};
};
thread_local AttemptScope* attemptScope{};
void prepareAttempt(UObject* service, void* entry, bool forced, bool eligible, AttemptScope& scope) {
    const auto key = entryKey(entry); if (!key) return;
    auto area = findArea(*key);
    if (area && area->object.valid()) {
        auto object = area->object.object;
        require(read<Array>(object, 0x350).count == 0, "quest-gated encounter excluded");
        auto entries = read<Array>(object, 0x340);
        require(arrayValid(entries, 64) && key->row < entries.count
            && entryReason(entries.data + key->row * 0x100) == SkipReason::None, "scripted encounter excluded");
    }
    Facts store(service); auto saved = DecisionJournal::locate(store, *key);
    // Absence means the builder did not authorize this encounter (including
    // quest-gated/scripted wildlife). Never infer eligibility from its name.
    if (!saved.decision) return;
    auto decision = *saved.decision;
    const auto state = members(entry);
    const auto phase = read<int16_t>(entry, 0x66);
    if (phase < 0) return;
    const auto phases = read<Array>(read<void*>(entry, 0x10), 0xd8);
    require(arrayValid(phases, 64) && phase < phases.count && read<uint32_t>(phases.data + phase * 0x40, 0x20) > 0,
        "respawn phase or positive quantity unavailable");
    // A restored choice may reach this callback after a builder budget skip.
    // Sanitize its existing owned points before any new members are created.
    if (state.count == 0) prepareRoamingPoints(decision);
    if (!forced && eligible && state.count == 0 && decision.phase == CyclePhase::Completed) {
        scope.newCycle = advanceCycle(decision, decision.cycle, RespawnCause::Natural, true, 0, options(key->definition), uniform);
        // Validate/import the target before advancing the save record. Failure
        // restores the previous complete profile and retains its saved cycle.
        prepareRoamingPoints(decision);
        setDefinition(read<void*>(entry, 0x10), decision, service);
        try { DecisionJournal::save(store, saved, decision); }
        catch (...) { setDefinition(read<void*>(entry, 0x10), *saved.decision, service); throw; }
    }
    setDefinition(read<void*>(entry, 0x10), decision, service);
    scope.entry = entry; scope.decision = decision; scope.newCycle = scope.newCycle || isFresh(*key);
}
void queued(UObject* service, UObject* stub) {
    if (!active || !configurationReady || failed || GetCurrentThreadId() != gameThread || !stub || !stub->IsA(stubType)) return;
    auto entry = read<void*>(stub, 0xe8); auto key = entryKey(entry); if (!key) return;
    if (!(read<uint8_t>(stub, 0x58) & 0x10)) return;
    auto state = members(entry, stub); if (!state.contains || state.living) return;
    Facts store(service); auto saved = DecisionJournal::locate(store, *key); if (!saved.decision) return;
    auto decision = *saved.decision;
    if (decision.phase == CyclePhase::Completed) return;
    if (acknowledgeCompletion(decision, decision.cycle, true, state.living)) DecisionJournal::save(store, saved, decision);
}

// The population registry owns the persisted dead bit. Check its existing
// entry before using the game's completion function; never fabricate a record.
unsigned char* registryRecord(UObject* population, uint32_t id) {
    require(population && population->IsA(static_cast<UClass*>(types[17].object)), "suppression population owner");
    const auto count = read<int32_t>(population, 0x358), free = read<int32_t>(population, 0x384);
    const auto hashSize = read<uint32_t>(population, 0x398);
    require(count > 0 && count <= 1000000 && free >= 0 && free < count && hashSize && hashSize <= 2097152
        && !(hashSize & (hashSize - 1)), "suppression registry bounds");
    auto hashes = read<unsigned char*>(population, 0x390);
    if (!hashes) hashes = reinterpret_cast<unsigned char*>(population) + 0x388;
    auto index = read<int32_t>(hashes, (id & (hashSize - 1)) * 4);
    auto data = read<unsigned char*>(population, 0x350); require(data != nullptr, "suppression registry missing");
    for (unsigned attempts = 0; index != -1 && attempts < 128; ++attempts) {
        require(index >= 0 && index < count, "suppression registry index");
        auto record = data + size_t(index) * 0xf0;
        if (read<uint32_t>(record, 0) == id) return record + 8;
        index = read<int32_t>(record, 0xe8);
    }
    throw std::runtime_error("new encounter stub is not registered");
}
void bindActivity(UObject* stub, int16_t phase, void* context, void* flags) {
    if (active && configurationReady && !failed && GetCurrentThreadId() == gameThread && attemptScope && attemptScope->decision
        && attemptScope->decision->outcome == Outcome::None) {
        try {
            require(stub && stub->IsA(stubType), "suppression stub class");
            auto entry = read<void*>(stub, 0xe8);
            if (entry == attemptScope->entry && !(read<uint8_t>(stub, 0x58) & 0x10)) {
                require(entryKey(entry) == std::optional{attemptScope->decision->key} && members(entry, stub).contains,
                    "suppression encounter registration");
                require(read<uint8_t>(stub, 0xd0) == 0 && read<void*>(stub, 0x38)
                    && read<int32_t>(stub, 0xe0) <= 0 && read<int32_t>(stub, 0xe4) == 0,
                    "suppression requires a new stub without a live actor or activity binding");
                auto population = read<UObject*>(stub, 0x170);
                auto record = registryRecord(population, read<uint32_t>(stub, 0x40));
                require(!(read<uint8_t>(record, 0x10) & 8), "suppression registry already completed");
                engine<CompleteStub>(0x5949c78)(stub);
                // The game's routine records the dead member and schedules its
                // original NextDay policy. Its d0==0 path touches no live actor.
                require((read<uint8_t>(stub, 0x58) & 0x10) && (read<uint8_t>(registryRecord(population, read<uint32_t>(stub, 0x40)), 0x10) & 8),
                    "suppression completion was not recorded");
                ++attemptScope->suppressed;
            }
        } catch (...) {
            attemptScope->suppressionFailed = true;
            if (logging) warning(L"No spawn completion failed validation; normal game processing retained.");
        }
    }
    // The original's first operation rejects completed stubs. Exactly one call,
    // with unchanged arguments, leaves its normal spawn/restore path intact.
    originalBind(stub, phase, context, flags);
}
void stop() {
    if (bindTarget) { MH_DisableHook(bindTarget); MH_RemoveHook(bindTarget); bindTarget = nullptr; }
    std::lock_guard guard(areaMutex); areaByIndex.clear(); areaByGuid.clear(); areaSlots.clear(); pruneArea = 0;
}
void start() {
    auto candidate = engine<void*>(0x144e8d4);
    require(MH_CreateHook(candidate, reinterpret_cast<void*>(&bindActivity), reinterpret_cast<void**>(&originalBind)) == MH_OK,
        "suppression activity hook creation failed");
    bindTarget = candidate;
    require(MH_EnableHook(bindTarget) == MH_OK, "suppression activity hook activation failed");
}
}
