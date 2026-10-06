#include "ReplacementPolicy.hpp"
#include "DecisionJournal.hpp"
#include <random>
#include "Transaction.hpp"
#include "WildlifeDefinitions.hpp"
#include "AppearanceRepair.hpp"
#include "RespawnEvidence.hpp"
#include <Mod/CppUserModBase.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <DynamicOutput/Output.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include "NativeContract.hpp"
#include "RespawnContract.hpp"
#include "LifecycleContract.hpp"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace LessWildlife {
using namespace RC::Unreal;
constexpr wchar_t wolf[] = L"/Game/_Dawnwalker/Combat/Enemies/Wolf/NPCDef_Wolf_Base.NPCDef_Wolf_Base_C";
constexpr wchar_t bandit[] = L"/Game/_Dawnwalker/Combat/Enemies/HumanEnemies/BanditBasic/NPCDef_BanditBasic_Normal.NPCDef_BanditBasic_Normal_C";
using Builder = void(*)(UObject*);
Builder original{};
void* target{};
using AppearanceInitializer = void(*)(UObject*, UObject*);
AppearanceInitializer originalAppearance{};
void* appearanceTarget{};
std::atomic_uint replacementMask{};
std::atomic_uint64_t replacementOptions{};
std::atomic_bool logging{}, active{}, failed{}, configurationReady{};
std::atomic_uint warningCount{};
DWORD gameThread{};
enum class SkipReason { None, Conditions, Scripted, Overrides, Table, Budget, Count };
struct Stats {
    uint64_t builders{}, wildlife{}, nonWildlife{}, cached{}, transformed{}, skipped{}, rolledBack{}, micros{}, maximum{}, reported{};
    std::array<uint64_t, static_cast<size_t>(SkipReason::Count)> reasons{};
} stats;
int64_t workFrame = -1; unsigned workEntries{}; uint64_t workMicros{};
UObject* clockOwner{}; UFunction* clockFunction{};

void message(const std::wstring& value) { RC::Output::send(L"[Less Wildlife native] " + value + L"\n"); }
void warning(const wchar_t* value) { if (warningCount.fetch_add(1) < 8) message(value); }
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void skip(UObject* area, SkipReason reason) {
    if (!logging) return;
    ++stats.skipped;
    auto count = ++stats.reasons[static_cast<size_t>(reason)];
    // Only two examples per reason per session; counters retain the totals.
    if (count > 2 || reason == SkipReason::Budget) return;
    constexpr const wchar_t* labels[] = {L"none", L"start conditions", L"scripted role/spawn points or no next-day respawn",
        L"authored AI or population extension", L"generated table unavailable", L"work budget"};
    message(L"Wildlife encounter retained: " + area->GetFullName() + L"; reason=" + labels[static_cast<size_t>(reason)]);
}
template<class T> T read(const void* data, size_t offset) {
    T value; std::memcpy(&value, static_cast<const unsigned char*>(data) + offset, sizeof(T)); return value;
}
struct Array { unsigned char* data; int32_t count, capacity; };
static_assert(sizeof(Array) == 16 && sizeof(FName) == 8);
static_assert(sizeof(TMap<FName, unsigned char*>) == 0x50);
bool arrayValid(const Array& a, int maximum) {
    return a.count >= 0 && a.count <= maximum && a.capacity >= a.count && a.capacity <= 65536 && (!a.count || a.data);
}

struct TypeRef {
    UObject* object{}; std::atomic_int index{-1}; int serial{}; std::atomic_bool alive{false};
    void bind(UObject* value) {
        require(value != nullptr, "population type missing");
        index = value->GetInternalIndex(); auto slot = FUObjectArray::IndexToObject(index);
        require(slot && slot->GetUObject() == value && FUObjectArray::IsValid(slot, false), "population type invalid");
        object = value; serial = slot->GetSerialNumber(); alive = true;
    }
    bool valid() const {
        if (!alive || index < 0) return false;
        auto slot = FUObjectArray::IndexToObject(index);
        return slot && slot->GetUObject() == object && FUObjectArray::IsValid(slot, false)
            && (!serial || serial == slot->GetSerialNumber());
    }
};
std::array<TypeRef, 19> types;
namespace EncounterRuntime { void invalidateArea(int32_t); }
TypeRef observerType;
std::atomic_bool listening{};
struct Listener final : FUObjectDeleteListener {
    void NotifyUObjectDeleted(const UObjectBase*, int32_t index) override {
        for (auto& type : types) if (type.index == index) { type.alive = false; active = false; }
        if (observerType.index == index) observerType.alive = false;
        EncounterRuntime::invalidateArea(index);
    }
    void OnUObjectArrayShutdown() override {
        active = false;
        if (listening.exchange(false)) FUObjectArray::RemoveUObjectDeleteListener(this);
    }
} listener;

FProperty* field(UStruct* owner, const wchar_t* name, int offset, int size, const wchar_t* kind) {
    auto property = owner->FindProperty(FName(name));
    auto mismatch = [&](const std::wstring& detail) {
        auto context = owner->GetFullName() + L":" + name + L" " + detail;
        std::string reason; reason.reserve(context.size());
        for (auto c : context) reason.push_back(c >= 0 && c <= 127 ? static_cast<char>(c) : '?');
        throw std::runtime_error(reason);
    };
    if (!property) mismatch(L"is missing");
    const auto actualOffset = property->GetOffset_Internal(), actualSize = property->GetSize();
    if (actualOffset != offset || actualSize != size)
        mismatch(L"layout mismatch: expected offset=" + std::to_wstring(offset) + L", size=" + std::to_wstring(size)
            + L"; found offset=" + std::to_wstring(actualOffset) + L", size=" + std::to_wstring(actualSize));
    const auto actualKind = property->GetClass().GetName();
    if (actualKind != kind) mismatch(L"type mismatch: expected " + std::wstring(kind) + L"; found " + actualKind);
    return property;
}
FProperty* definitionProperty{};
FArrayProperty* montageProperty{};
FBoolProperty* hostileProperty{};
UStruct* entryType{};
UStruct* rowType{};
UClass* areaType{};
UClass* stubType{};
UClass* humanoidType{};
FBoolProperty* randomAppearanceProperty{};
FName banditClassName;
struct DefinitionKey {
    FName package, asset;
    bool operator==(const DefinitionKey& other) const { return package == other.package && asset == other.asset; }
};
std::array<DefinitionKey, wildlifeDefinitions.size()> wildlifeKeys;
DefinitionKey banditKey;
constexpr const wchar_t* enemies[] = {bandit,
 L"/Game/_Dawnwalker/Combat/Enemies/BloodSlave/NPCDef_BloodSlave_Base.NPCDef_BloodSlave_Base_C",
 L"/Game/_Dawnwalker/Combat/Enemies/Vidmo/NPCDef_Vidmo_Base.NPCDef_Vidmo_Base_C",
 L"/Game/_Dawnwalker/Combat/Enemies/EnemyVariants/Kobold/NPCDef_Kobold_Forest_Combat.NPCDef_Kobold_Forest_Combat_C"};
std::array<DefinitionKey, 4> enemyKeys;

void bindDefinitionKeys() {
    auto key = [](std::wstring_view path) {
        const auto dot = path.rfind(L'.');
        require(dot != std::wstring_view::npos, "invalid definition path");
        return DefinitionKey{FName(std::wstring(path.substr(0, dot)).c_str()), FName(std::wstring(path.substr(dot + 1)).c_str())};
    };
    for (size_t i = 0; i < wildlifeKeys.size(); ++i) wildlifeKeys[i] = key(wildlifeDefinitions[i].path);
    banditKey = key(bandit);
    for (size_t i = 0; i < enemyKeys.size(); ++i) enemyKeys[i] = key(enemies[i]);
}

void bindSchema() {
    auto find = [](const wchar_t* path) { return UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path); };
    types[0].bind(find(L"/Script/Population.PopulationArea"));
    types[1].bind(find(L"/Script/Population.PopulationAreaEntry"));
    types[2].bind(find(L"/Script/Population.CommunityRow"));
    types[3].bind(find(L"/Script/Engine.Default__KismetSystemLibrary"));
    clockOwner = types[3].object;
    types[4].bind(clockOwner->GetFunctionByNameInChain(L"GetFrameCount"));
    clockFunction = static_cast<UFunction*>(types[4].object);
    require(clockFunction->GetParmsSize() == 8, "frame counter parameters changed");
    field(clockFunction, L"ReturnValue", 0, 8, L"Int64Property");
    types[5].bind(find(L"/Script/CoreUObject.SoftObjectPath"));
    types[6].bind(find(L"/Script/DogwoodAI.DogwoodAIStub"));
    types[7].bind(find(L"/Script/Dawnwalker.HumanoidNPCDefinition"));
    stubType = static_cast<UClass*>(types[6].object);
    humanoidType = static_cast<UClass*>(types[7].object);
    require(stubType->GetPropertiesSize() == 0x270, "AI stub layout changed");
    randomAppearanceProperty = static_cast<FBoolProperty*>(field(humanoidType, L"bRandomizeAppearance", 0x4f8, 1, L"BoolProperty"));
    field(humanoidType, L"RandomizedAppearanceTable", 0x508, 8, L"ObjectProperty");
    banditClassName = FName(L"NPCDef_BanditBasic_Normal_C");
    auto softType = static_cast<UStruct*>(types[5].object);
    require(softType->GetPropertiesSize() == 32, "soft reference path size changed");
    field(softType, L"AssetPath", 0, 16, L"StructProperty");
    field(softType, L"SubPathString", 16, 16, L"StrProperty");
    areaType = static_cast<UClass*>(types[0].object);
    entryType = static_cast<UStruct*>(types[1].object);
    rowType = static_cast<UStruct*>(types[2].object);
    require(areaType->GetPropertiesSize() == 0x618 && entryType->GetPropertiesSize() == 0x100
            && rowType->GetPropertiesSize() == 0x118, "population struct size changed");
    field(areaType, L"Entries", 0x340, 16, L"ArrayProperty");
    field(areaType, L"StartConditions", 0x350, 16, L"ArrayProperty");
    field(areaType, L"ActiveEntries", 0x360, 16, L"ArrayProperty");
    field(areaType, L"GeneratedDataTable", 0x370, 8, L"ObjectProperty");
    field(areaType, L"DynamicSpawnPoints", 0x388, 16, L"ArrayProperty");
    field(areaType, L"DynamicActionPoints", 0x398, 16, L"ArrayProperty");
    field(entryType, L"PawnDefinition", 0, 40, L"SoftClassProperty");
    field(entryType, L"AIDefinition", 0x28, 8, L"ClassProperty");
    field(entryType, L"AIReactions", 0x30, 8, L"ClassProperty");
    field(entryType, L"AIFaction", 0x38, 8, L"StructProperty");
    field(entryType, L"AITags", 0x40, 32, L"StructProperty");
    hostileProperty = static_cast<FBoolProperty*>(field(entryType, L"bIsHostile", 0x60, 1, L"BoolProperty"));
    field(entryType, L"NPCRole", 0x62, 1, L"EnumProperty");
    field(entryType, L"RespawnPolicy", 0x63, 1, L"EnumProperty");
    field(entryType, L"Behavior", 0x64, 1, L"EnumProperty");
    field(entryType, L"Quantity", 0x68, 4, L"UInt32Property");
    field(entryType, L"MaxQuantity", 0x6c, 4, L"UInt32Property");
    montageProperty = static_cast<FArrayProperty*>(field(entryType, L"Montages", 0xb0, 16, L"ArrayProperty"));
    require(FArrayProperty::MemberOffsets.at(L"Inner") == 0x78, "montage array property layout changed");
    auto montageInner = montageProperty->GetInner();
    require(montageInner && montageInner->GetSize() == 40 && montageInner->GetClass().GetName() == L"SoftObjectProperty",
            "montage array element layout changed");
    field(entryType, L"PopulationExtensionConfig", 0xc8, 16, L"StructProperty");
    definitionProperty = field(rowType, L"PawnDefinition", 0x30, 40, L"SoftClassProperty");
    field(rowType, L"PawnClass", 8, 40, L"SoftClassProperty");
    field(rowType, L"AIDefinition", 0x58, 8, L"ClassProperty");
    field(rowType, L"AIReactions", 0x60, 8, L"ClassProperty");
    field(rowType, L"AIFaction", 0x68, 8, L"StructProperty");
    field(rowType, L"AITags", 0x70, 32, L"StructProperty");
    field(rowType, L"NPCRole", 0x90, 1, L"EnumProperty");
    field(rowType, L"RespawnPolicy", 0xc0, 1, L"EnumProperty");
    require(UDataTable::MemberOffsets.at(L"RowMap") == 0x30, "data table map layout unavailable");
    bindDefinitionKeys();
}

// Read only the independently checked engine representation. Never construct a
// UE4SS TSoftObjectPtr/FWeakObjectPtr or allocate an object serial.
std::wstring softPath(const void* value) {
    require(read<int32_t>(value, 32) == 0, "subobject soft reference not supported");
    auto package = read<FName>(value, 8).ToString();
    auto asset = read<FName>(value, 16).ToString();
    return package + L"." + asset;
}
DefinitionKey definitionKey(const void* value) {
    require(read<int32_t>(value, 32) == 0, "subobject soft reference not supported");
    return {read<FName>(value, 8), read<FName>(value, 16)};
}
Species definitionSpecies(const DefinitionKey& key) {
    for (size_t i = 0; i < wildlifeKeys.size(); ++i) if (key == wildlifeKeys[i]) return wildlifeDefinitions[i].species;
    return Species::None;
}
bool emptyTags(const void* value) {
    return read<int32_t>(value, 8) == 0 && read<int32_t>(value, 24) == 0;
}
SkipReason entryReason(const void* entry) {
    if (read<uint8_t>(entry, 0x62) != 3 || read<uint8_t>(entry, 0x63) != 1 || read<uint8_t>(entry, 0x64) != 0)
        return SkipReason::Scripted;
    if (read<uint64_t>(entry, 0xc8) || read<uint64_t>(entry, 0x28) || read<uint64_t>(entry, 0x30)
        || read<uint64_t>(entry, 0x38) || !emptyTags(static_cast<const unsigned char*>(entry) + 0x40))
        return SkipReason::Overrides;
    return SkipReason::None;
}
struct Value {
    FProperty* property; void* data;
    explicit Value(FProperty* p) : property(p), data(p->AllocateAndInitializeValue()) { require(data != nullptr, "property allocation failed"); }
    ~Value() { property->DestroyAndFreeValue(data); }
    Value(const Value&) = delete; Value& operator=(const Value&) = delete;
};
struct Operation {
    void* destination; void* activeEntry; Value backup; Value montageBackup;
    const Value* replacement; const Value* emptyMontages; bool wasHostile;
    Operation(void* row, void* entry, const Value& next, const Value& noMontages)
        : destination(static_cast<unsigned char*>(row) + 0x30), activeEntry(entry), backup(definitionProperty), montageBackup(montageProperty),
          replacement(&next), emptyMontages(&noMontages),
          wasHostile(hostileProperty->GetPropertyValue(static_cast<unsigned char*>(entry) + 0x60)) {
        definitionProperty->CopyCompleteValue(backup.data, destination);
        montageProperty->CopyCompleteValue(montageBackup.data, static_cast<unsigned char*>(activeEntry) + 0xb0);
    }
    void apply() {
        definitionProperty->CopyCompleteValue(destination, replacement->data);
        hostileProperty->SetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60, true);
        // RandomPoints entries can still supply animal sleeping/eating/sitting
        // montages. Their dynamic activity points are built after this callback.
        // An empty list uses the engine's normal montage-free roaming points.
        montageProperty->CopyCompleteValue(static_cast<unsigned char*>(activeEntry) + 0xb0, emptyMontages->data);
    }
    bool readbackMatches() {
        return definitionKey(destination) == definitionKey(replacement->data) && hostileProperty->GetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60)
            && montageProperty->Identical(static_cast<unsigned char*>(activeEntry) + 0xb0, emptyMontages->data);
    }
    void restore() {
        definitionProperty->CopyCompleteValue(destination, backup.data);
        hostileProperty->SetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60, wasHostile);
        montageProperty->CopyCompleteValue(static_cast<unsigned char*>(activeEntry) + 0xb0, montageBackup.data);
    }
    bool restored() {
        return definitionKey(destination) == definitionKey(backup.data) && hostileProperty->GetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60) == wasHostile
            && montageProperty->Identical(static_cast<unsigned char*>(activeEntry) + 0xb0, montageBackup.data);
    }
};
// Stable addresses and RAII engine values; no borrowed row survives this call.
struct OperationList {
    std::vector<std::unique_ptr<Operation>> values;
    struct Iterator { decltype(values.begin()) it; Operation& operator*() const { return **it; } void operator++(){++it;} bool operator!=(const Iterator& o)const{return it!=o.it;} };
    Iterator begin(){return {values.begin()};} Iterator end(){return {values.end()};}
    Operation& operator[](size_t n){return *values[n];}
};

#include "EncounterRuntime.inl"

void transform(UObject* area) {
    for (const auto& type : types) require(type.valid(), "population metadata expired");
    require(area && area->IsA(areaType), "builder owner is not a population area");
    auto entries = read<Array>(area, 0x340), activeEntries = read<Array>(area, 0x360);
    require(arrayValid(entries, 64) && arrayValid(activeEntries, 64), "population entry limit or layout");
    auto selected = [](const DefinitionKey& key) { return definitionSpecies(key) != Species::None; };
    bool hasWildlife = false;
    for (int i = 0; i < entries.count; ++i) if (selected(definitionKey(entries.data + i * 0x100))) { hasWildlife = true; break; }
    if (!hasWildlife) { if (logging) ++stats.nonWildlife; return; }
    if (logging) ++stats.wildlife;
    auto conditions = read<Array>(area, 0x350);
    require(arrayValid(conditions, 64), "population start condition layout");
    if (conditions.count) { skip(area, SkipReason::Conditions); return; }
    const bool pointsBuilt = read<Array>(area, 0x388).count != 0 || read<Array>(area, 0x398).count != 0;
    // Guard areas also bound ordinary roaming packs. Preserve the game's
    // boundary and registration data; attached scripted spawn points are
    // excluded by the entry behavior/role/respawn checks below.
    auto table = read<UDataTable*>(area, 0x370);
    if (!table || table->GetRowStruct() != rowType) { skip(area, SkipReason::Table); return; }
    auto& rows = table->GetRowMap();
    require(rows.Num() == activeEntries.count && rows.Num() <= 64 && rows.GetMaxIndex() <= 64, "generated row mapping changed");
    // Finish eligibility and row validation before any engine property import
    // or backup allocation. Scripted or mixed groups incur no preparation work.
    struct PlannedRow { void* row; void* entry; EncounterKey key; };
    std::array<PlannedRow, 64> planned{};
    size_t plannedCount = 0;
    unsigned ordinal = 0, boarRows = 0, wolfRows = 0;
    for (auto& pair : rows) {
        auto row = pair.Value;
        auto current = activeEntries.data + ordinal++ * 0x100;
        auto key = pair.Key.ToString();
        auto separator = key.find(L'_');
        require(separator != std::wstring::npos && separator > 0 && separator <= 2, "unexpected population row identity");
        unsigned index = 0;
        for (size_t j = 0; j < separator; ++j) { require(key[j] >= L'0' && key[j] <= L'9', "invalid row index"); index = index * 10 + key[j] - L'0'; }
        require(index < static_cast<unsigned>(entries.count), "population source row index outside entries");
        auto source = entries.data + index * 0x100;
        auto sourcePath = definitionKey(source);
        require(definitionKey(current) == sourcePath, "active/source row mapping mismatch");
        if (!selected(sourcePath)) continue;
        // Plan every selected row before writes so an ineligible member cannot
        // leave a partially converted encounter. Disabled species stay intact.
        auto reason = entryReason(source);
        if (reason == SkipReason::None) reason = entryReason(current);
        if (reason != SkipReason::None) { skip(area, reason); return; }
        require(arrayValid(read<Array>(current, 0xb0), 64), "activity montage list limit or layout");
        require((definitionKey(row + 0x30) == sourcePath || std::find(enemyKeys.begin(), enemyKeys.end(), definitionKey(row + 0x30)) != enemyKeys.end())
                && read<uint8_t>(row, 0x90) == 3 && read<uint8_t>(row, 0xc0) == 1,
                "generated wildlife row changed externally");
        require(read<uint64_t>(row, 0x10) == 0 && read<uint64_t>(row, 0x58) == 0 && read<uint64_t>(row, 0x60) == 0
                && read<uint64_t>(row, 0x68) == 0 && emptyTags(row + 0x70), "authored wildlife AI overrides need explicit handling");
        auto identity = EncounterRuntime::identify(read<std::array<uint32_t, 4>>(area, 0x1e0), pair.Key);
        require(identity && identity->row == index && wildlifeKeys[identity->definition] == sourcePath, "encounter stable identity mismatch");
        planned[plannedCount++] = {row, current, *identity};
        if (definitionSpecies(sourcePath) == Species::Boar) ++boarRows; else ++wolfRows;
    }
    if (!plannedCount) return;
    EncounterRuntime::rememberArea(area);
    EncounterRuntime::Facts store(area);
    struct Prepared {
        DecisionJournal::Slot saved;
        EncounterDecision decision;
        std::unique_ptr<Value> replacement;
    };
    std::vector<Prepared> prepared; prepared.reserve(plannedCount);
    Value noMontages(montageProperty);
    OperationList operations; operations.values.reserve(plannedCount);
    for (size_t i = 0; i < plannedCount; ++i) {
        auto& plan = planned[i];
        auto saved = DecisionJournal::locate(store, plan.key);
        auto decision = restoreOrBegin(plan.key, saved.decision, EncounterRuntime::options(plan.key.definition), EncounterRuntime::uniform);
        // Adopt already converted cached rows from the earlier bandit helper.
        if (!saved.decision && definitionKey(static_cast<unsigned char*>(plan.row) + 0x30) == banditKey)
            decision = {plan.key, 0, Outcome::Bandit, CyclePhase::Active};
        auto replacement = std::make_unique<Value>(definitionProperty);
        auto end = definitionProperty->ImportText_Direct(EncounterRuntime::path(decision), replacement->data, area, 0, nullptr);
        require(end && !*end && definitionKey(replacement->data) == EncounterRuntime::keyFor(decision), "outcome soft reference import failed");
        if (decision.outcome != Outcome::Original && decision.outcome != Outcome::None) {
            if (pointsBuilt) {
                require(definitionKey(static_cast<unsigned char*>(plan.row) + 0x30) == EncounterRuntime::keyFor(decision),
                    "cached encounter changed before registration");
            } else operations.values.push_back(std::make_unique<Operation>(plan.row, plan.entry, *replacement, noMontages));
        }
        prepared.push_back({std::move(saved), decision, std::move(replacement)});
    }
    // Save before registration; no choice is kept in an external/global save.
    for (auto& item : prepared) if (!item.saved.decision) {
        DecisionJournal::save(store, item.saved, item.decision);
        EncounterRuntime::noteFresh(item.decision.key);
    }
    const auto result = commit(operations);
    if (result != TransactionResult::Applied) {
        // The row transaction retained animals. Commit that result to this save
        // as well, so a later overlap cannot silently roll again.
        for (auto& item : prepared) {
            auto original = item.decision; original.outcome = Outcome::Original; original.phase = CyclePhase::Active;
            DecisionJournal::save(store, item.saved, original);
        }
        if (logging) ++stats.rolledBack;
        if (result == TransactionResult::RollbackFailed) { failed = true; warning(L"Replacement disabled after an unverifiable rollback."); }
        return;
    }
    if (logging) {
        ++stats.transformed;
        if (stats.transformed <= 12) message(L"Saved encounter outcomes: " + area->GetFullName() + L"; groups=" + std::to_wstring(plannedCount)
            + L"; original identities, counts and respawn policies retained.");
    }
}

void builder(UObject* area) {
    // Always run the game's builder exactly once. It also handles its cached
    // table. Cached rows reuse saved decisions and never trigger another roll.
    const bool participate = active && configurationReady && !failed && GetCurrentThreadId() == gameThread;
    auto prior = participate && area ? read<UObject*>(area, 0x370) : nullptr;
    original(area);
    if (!participate) return;
    if (prior && logging) ++stats.cached;
    auto started = std::chrono::steady_clock::now();
    if (logging) ++stats.builders;
    try {
        require(types[3].valid() && types[4].valid(), "frame counter expired");
        int64_t frame = -1; clockOwner->ProcessEvent(clockFunction, &frame);
        require(frame >= 0, "invalid frame counter");
        if (frame != workFrame) { workFrame = frame; workEntries = 0; workMicros = 0; }
        auto count = read<Array>(area, 0x340).count;
        // Retain only indexed identity here, even if the table work must wait.
        // A saved choice can then validate this area's quest gates and owned
        // action points when the normal attempt callback restores it.
        if (area && area->IsA(areaType) && count >= 0 && count <= 64) EncounterRuntime::rememberArea(area);
        if (count < 0 || count > 64 || workEntries + count > 128 || workMicros >= 2000) { skip(area, SkipReason::Budget); return; }
        workEntries += count;
        transform(area);
    }
    catch (const std::exception& error) {
        if (logging) ++stats.skipped;
        if (logging && warningCount.fetch_add(1) < 8) { std::string s(error.what()); message(L"Encounter unchanged: " + std::wstring(s.begin(), s.end())); }
    }
    auto us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
    workMicros += us;
    if (logging) {
        stats.micros += us; stats.maximum = std::max(stats.maximum, us);
        auto now = GetTickCount64();
        if (now - stats.reported >= 10000) {
            stats.reported = now;
            message(L"Encounter totals: new tables=" + std::to_wstring(stats.builders) + L", replaced=" + std::to_wstring(stats.transformed)
                + L", skipped=" + std::to_wstring(stats.skipped) + L", rollback=" + std::to_wstring(stats.rolledBack)
                + L", wildlife tables=" + std::to_wstring(stats.wildlife) + L", other tables=" + std::to_wstring(stats.nonWildlife)
                + L", cached=" + std::to_wstring(stats.cached)
                + L", conditions=" + std::to_wstring(stats.reasons[static_cast<size_t>(SkipReason::Conditions)])
                + L", scripted=" + std::to_wstring(stats.reasons[static_cast<size_t>(SkipReason::Scripted)])
                + L", overrides=" + std::to_wstring(stats.reasons[static_cast<size_t>(SkipReason::Overrides)])
                + L", table unavailable=" + std::to_wstring(stats.reasons[static_cast<size_t>(SkipReason::Table)])
                + L", budget=" + std::to_wstring(stats.reasons[static_cast<size_t>(SkipReason::Budget)])
                + L", total us=" + std::to_wstring(stats.micros) + L", max us=" + std::to_wstring(stats.maximum));
        }
    }
}

struct AppearanceStats { uint64_t checked{}, repaired{}, micros{}, maximum{}, reported{}; } appearanceStats;
void initializeAppearance(UObject* definition, UObject* stub) {
    bool repaired = false, measured = false;
    auto started = std::chrono::steady_clock::time_point{};
    try {
        if (active && GetCurrentThreadId() == gameThread && definition && stub
            && types[6].valid() && types[7].valid()
            && definition->IsA(humanoidType) && stub->IsA(stubType)
            && EncounterRuntime::enemyDefinition(definition)
            && randomAppearanceProperty->GetPropertyValue(reinterpret_cast<unsigned char*>(definition) + 0x4f8)) {
            const auto name = stub->GetFName();
            const auto row = FName(name.GetComparisonIndex().ToUnstableInt(), 0).ToString();
            if (wildlifeRow(row)) {
                measured = logging;
                if (measured) { started = std::chrono::steady_clock::now(); ++appearanceStats.checked; }
                // Runs before the normal humanoid initializer, including saved
                // encounters. No actor, CDO, shared appearance or inventory is
                // replaced. The engine chooses and persists the bandit preset.
                auto record = read<void*>(stub, 0x38);
                auto scope = EncounterRuntime::attemptScope;
                if (record && scope && scope->decision && scope->newCycle && read<void*>(stub, 0xe8) == scope->entry) {
                    std::memset(static_cast<unsigned char*>(record) + 0x24, 0, 3);
                    repaired = true;
                } else repaired = clearAnimalAppearance(record);
            }
        }
    } catch (const std::exception&) {
        if (logging) warning(L"Could not check a replacement appearance; normal initialization retained.");
    }
    if (measured) {
        const auto us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
        appearanceStats.micros += us; appearanceStats.maximum = std::max(appearanceStats.maximum, us);
        if (repaired) ++appearanceStats.repaired;
    }
    originalAppearance(definition, stub);
    if (measured && logging) {
        if (repaired && appearanceStats.repaired <= 2) message(L"Reset inherited appearance data before normal humanoid initialization.");
        const auto now = GetTickCount64();
        if (now - appearanceStats.reported >= 10000) {
            appearanceStats.reported = now;
            message(L"Appearance checks=" + std::to_wstring(appearanceStats.checked) + L", animal records cleared=" + std::to_wstring(appearanceStats.repaired)
                + L", repair us=" + std::to_wstring(appearanceStats.micros) + L", max repair us=" + std::to_wstring(appearanceStats.maximum));
        }
    }
}

#include "RespawnObserver.inl"

bool start() {
    if (active) return true;
    if (target || appearanceTarget || failed) return false;
    try {
        bindSchema();
        EncounterRuntime::bind();
        std::wstring reason;
        auto candidate = NativeContract::resolve(reason);
        if (!candidate) { message(L"Replacement unavailable: " + reason); return false; }
        auto status = MH_Initialize();
        require(status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED, "MinHook initialization failed");
        auto appearanceCandidate = NativeContract::appearanceInitializer();
        require(MH_CreateHook(appearanceCandidate, reinterpret_cast<void*>(&initializeAppearance), reinterpret_cast<void**>(&originalAppearance)) == MH_OK, "appearance initializer hook creation failed");
        appearanceTarget = appearanceCandidate;
        require(MH_CreateHook(candidate, reinterpret_cast<void*>(&builder), reinterpret_cast<void**>(&original)) == MH_OK, "population builder hook creation failed");
        target = candidate; gameThread = GetCurrentThreadId();
        if (!listening.exchange(true)) FUObjectArray::AddUObjectDeleteListener(&listener);
        active = true;
        require(MH_EnableHook(appearanceTarget) == MH_OK, "appearance initializer hook activation failed");
        require(MH_EnableHook(target) == MH_OK, "population builder hook activation failed");
        EncounterRuntime::start();
        RespawnObserver::start();
        require(RespawnObserver::enabled, "encounter lifecycle hooks unavailable");
        if (logging) message(L"Encounter replacement ready: saved choices, natural cycles and suppression.");
        return true;
    } catch (const std::exception& error) {
        active = false;
        RespawnObserver::stop();
        EncounterRuntime::stop();
        if (target) { MH_DisableHook(target); MH_RemoveHook(target); target = nullptr; }
        if (appearanceTarget) { MH_DisableHook(appearanceTarget); MH_RemoveHook(appearanceTarget); appearanceTarget = nullptr; }
        if (listening.exchange(false)) FUObjectArray::RemoveUObjectDeleteListener(&listener);
        if (warningCount.fetch_add(1) < 8) { std::string s(error.what()); message(L"Replacement unavailable: " + std::wstring(s.begin(), s.end())); }
        return false;
    }
}
void stop() {
    active = false;
    RespawnObserver::stop();
    EncounterRuntime::stop();
    if (target) { MH_DisableHook(target); MH_RemoveHook(target); target = nullptr; }
    if (appearanceTarget) { MH_DisableHook(appearanceTarget); MH_RemoveHook(appearanceTarget); appearanceTarget = nullptr; }
    if (listening.exchange(false)) FUObjectArray::RemoveUObjectDeleteListener(&listener);
    // Other UE4SS mods can share MinHook. Do not uninitialize their hooks.
}
}

using namespace RC;
class LessWildlifeMod final : public CppUserModBase {
public:
    LessWildlifeMod() { ModName = L"Less Wildlife"; ModVersion = L"0.0.0"; ModAuthors = L"oOCamilleOo"; ModDescription = L"Population adjustment and encounter replacement."; }
    void on_lua_start(StringViewType name, LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) override {
        if (name != L"LessWildlife") return;
        lua.register_function("_LWConfigureReplacementV1", [](const auto& l) {
            // LuaMadeSimple removes each consumed argument; every read is index 1.
            auto boars = l.get_integer(1); auto wolves = l.get_integer(1);
            auto boarPool = l.get_integer(1); auto wolfPool = l.get_integer(1); auto logs = l.get_integer(1);
            LessWildlife::EncounterRuntime::configure(boars, wolves, boarPool, wolfPool);
            LessWildlife::logging = logs == 1; return 0;
        });
        lua.register_function("_LWStartReplacementV1", [](const auto& l) { l.set_bool(LessWildlife::start()); return 1; });
    }
    void on_lua_stop(StringViewType name, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) override {
        if (name == L"LessWildlife") LessWildlife::configurationReady = false;
    }
    ~LessWildlifeMod() override { LessWildlife::stop(); }
};
extern "C" __declspec(dllexport) CppUserModBase* start_mod() { return new LessWildlifeMod; }
extern "C" __declspec(dllexport) void uninstall_mod(CppUserModBase* mod) { delete mod; }
