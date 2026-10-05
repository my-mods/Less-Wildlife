#include "ReplacementPolicy.hpp"
#include "Transaction.hpp"
#include <Mod/CppUserModBase.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <DynamicOutput/Output.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include "NativeContract.hpp"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace LessWildlife {
using namespace RC::Unreal;
constexpr wchar_t wolf[] = L"/Game/_Dawnwalker/Combat/Enemies/Wolf/NPCDef_Wolf_Base.NPCDef_Wolf_Base_C";
constexpr wchar_t bandit[] = L"/Game/_Dawnwalker/Combat/Enemies/HumanEnemies/BanditBasic/NPCDef_BanditBasic_Normal.NPCDef_BanditBasic_Normal_C";
using Builder = void(*)(UObject*);
Builder original{};
void* target{};
std::atomic_bool enabled{}, logging{}, active{}, failed{};
std::atomic_uint warningCount{};
DWORD gameThread{};
struct Stats { uint64_t builders{}, transformed{}, skipped{}, rolledBack{}, micros{}, maximum{}, reported{}; } stats;
int64_t workFrame = -1; unsigned workEntries{}; uint64_t workMicros{};
UObject* clockOwner{}; UFunction* clockFunction{};

void message(const std::wstring& value) { RC::Output::send(L"[Less Wildlife native] " + value + L"\n"); }
void warning(const wchar_t* value) { if (warningCount.fetch_add(1) < 8) message(value); }
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
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
std::array<TypeRef, 6> types;
std::atomic_bool listening{};
struct Listener final : FUObjectDeleteListener {
    void NotifyUObjectDeleted(const UObjectBase*, int32_t index) override {
        for (auto& type : types) if (type.index == index) { type.alive = false; active = false; }
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
FBoolProperty* hostileProperty{};
UStruct* entryType{};
UStruct* rowType{};
UClass* areaType{};

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
    field(areaType, L"bUseAttachedGuardArea", 0x330, 1, L"BoolProperty");
    field(areaType, L"GuardArea", 0x334, 8, L"WeakObjectProperty");
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
}

// Read only the independently checked engine representation. Never construct a
// UE4SS TSoftObjectPtr/FWeakObjectPtr or allocate an object serial.
std::wstring softPath(const void* value) {
    require(read<int32_t>(value, 32) == 0, "subobject soft reference not supported");
    auto package = read<FName>(value, 8).ToString();
    auto asset = read<FName>(value, 16).ToString();
    return package + L"." + asset;
}
bool emptyTags(const void* value) {
    return read<int32_t>(value, 8) == 0 && read<int32_t>(value, 24) == 0;
}
bool noGuardArea(const void* area) {
    // Inspect the validated weak-reference storage without constructing a
    // wrapper or resolving an object. Both engine null representations have
    // a zero serial; retain any authored, stale or malformed reference.
    const auto index = read<int32_t>(area, 0x334), serial = read<int32_t>(area, 0x338);
    return (index == 0 || index == -1) && serial == 0;
}
bool ambient(const void* entry) {
    return read<uint8_t>(entry, 0x62) == 3 && read<uint8_t>(entry, 0x63) == 1
        && read<uint8_t>(entry, 0x64) == 0 && read<uint64_t>(entry, 0xc8) == 0
        && read<uint64_t>(entry, 0x28) == 0 && read<uint64_t>(entry, 0x30) == 0
        && read<uint64_t>(entry, 0x38) == 0 && emptyTags(static_cast<const unsigned char*>(entry) + 0x40);
}
struct Value {
    FProperty* property; void* data;
    explicit Value(FProperty* p) : property(p), data(p->AllocateAndInitializeValue()) { require(data != nullptr, "property allocation failed"); }
    ~Value() { property->DestroyAndFreeValue(data); }
    Value(const Value&) = delete; Value& operator=(const Value&) = delete;
};
struct Operation {
    void* destination; void* activeEntry; Value backup; const Value* replacement; bool wasHostile;
    Operation(void* row, void* entry, const Value& next)
        : destination(static_cast<unsigned char*>(row) + 0x30), activeEntry(entry), backup(definitionProperty), replacement(&next),
          wasHostile(hostileProperty->GetPropertyValue(static_cast<unsigned char*>(entry) + 0x60)) {
        definitionProperty->CopyCompleteValue(backup.data, destination);
    }
    void apply() {
        definitionProperty->CopyCompleteValue(destination, replacement->data);
        hostileProperty->SetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60, true);
    }
    bool readbackMatches() { return softPath(destination) == bandit && hostileProperty->GetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60); }
    void restore() {
        definitionProperty->CopyCompleteValue(destination, backup.data);
        hostileProperty->SetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60, wasHostile);
    }
    bool restored() { return softPath(destination) == wolf && hostileProperty->GetPropertyValue(static_cast<unsigned char*>(activeEntry) + 0x60) == wasHostile; }
};
// Stable addresses and RAII engine values; no borrowed row survives this call.
struct OperationList {
    std::vector<std::unique_ptr<Operation>> values;
    struct Iterator { decltype(values.begin()) it; Operation& operator*() const { return **it; } void operator++(){++it;} bool operator!=(const Iterator& o)const{return it!=o.it;} };
    Iterator begin(){return {values.begin()};} Iterator end(){return {values.end()};}
    Operation& operator[](size_t n){return *values[n];}
};

void transform(UObject* area) {
    for (const auto& type : types) require(type.valid(), "population metadata expired");
    require(area && area->IsA(areaType), "builder owner is not a population area");
    auto conditions = read<Array>(area, 0x350);
    if (!arrayValid(conditions, 64) || conditions.count || read<uint8_t>(area, 0x330)
        || !noGuardArea(area)) { if (logging) ++stats.skipped; return; }
    auto entries = read<Array>(area, 0x340), activeEntries = read<Array>(area, 0x360);
    require(arrayValid(entries, 64) && arrayValid(activeEntries, 64), "population entry limit or layout");
    bool hasWolf = false;
    for (int i = 0; i < entries.count; ++i) if (softPath(entries.data + i * 0x100) == wolf) { hasWolf = true; break; }
    if (!hasWolf) return;
    auto table = read<UDataTable*>(area, 0x370);
    if (!table || table->GetRowStruct() != rowType) { if (logging) ++stats.skipped; return; }
    auto& rows = table->GetRowMap();
    require(rows.Num() == activeEntries.count && rows.Num() <= 64 && rows.GetMaxIndex() <= 64, "generated row mapping changed");
    Value replacement(definitionProperty);
    auto end = definitionProperty->ImportText_Direct(bandit, replacement.data, area, 0, nullptr);
    require(end && !*end && softPath(replacement.data) == bandit, "bandit soft reference import failed");
    OperationList operations;
    unsigned ordinal = 0;
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
        require(softPath(current) == softPath(source), "active/source row mapping mismatch");
        if (softPath(source) != wolf) continue;
        // One decision covers every wolf row. An ineligible member prevents a
        // partial conversion of a pack; other species are never changed.
        if (!ambient(source) || !ambient(current)) { if (logging) ++stats.skipped; return; }
        require(softPath(row + 0x30) == wolf && read<uint8_t>(row, 0x90) == 3 && read<uint8_t>(row, 0xc0) == 1,
                "generated wolf row changed externally");
        require(read<uint64_t>(row, 0x10) == 0 && read<uint64_t>(row, 0x58) == 0 && read<uint64_t>(row, 0x60) == 0
                && read<uint64_t>(row, 0x68) == 0 && emptyTags(row + 0x70), "authored wolf AI overrides need explicit handling");
        operations.values.push_back(std::make_unique<Operation>(row, current, replacement));
    }
    if (operations.values.empty()) return;
    auto result = commit(operations);
    if (result == TransactionResult::RollbackFailed) { failed = true; warning(L"Replacement disabled: rollback could not be verified. Population controls remain available."); }
    else if (result == TransactionResult::RolledBack) { if (logging) ++stats.rolledBack; warning(L"Replacement rolled back; original encounter retained."); }
    else {
        if (logging) ++stats.transformed;
        if (logging) message(L"Prototype wolf -> bandit: " + area->GetFullName() + L"; rows=" + std::to_wstring(operations.values.size()) + L"; original row keys/counts/respawn retained.");
    }
}

void builder(UObject* area) {
    // Always run the game's builder exactly once. It also handles its cached
    // table. Never change an already registered/cached encounter in this gate.
    const bool participate = active && enabled && !failed && GetCurrentThreadId() == gameThread;
    auto prior = participate && area ? read<UObject*>(area, 0x370) : nullptr;
    original(area);
    if (!participate || prior) return;
    auto started = std::chrono::steady_clock::now();
    if (logging) ++stats.builders;
    try {
        require(types[3].valid() && types[4].valid(), "frame counter expired");
        int64_t frame = -1; clockOwner->ProcessEvent(clockFunction, &frame);
        require(frame >= 0, "invalid frame counter");
        if (frame != workFrame) { workFrame = frame; workEntries = 0; workMicros = 0; }
        auto count = read<Array>(area, 0x340).count;
        if (count < 0 || count > 64 || workEntries + count > 128 || workMicros >= 2000) { if (logging) ++stats.skipped; return; }
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
            message(L"Prototype totals: new tables=" + std::to_wstring(stats.builders) + L", replaced=" + std::to_wstring(stats.transformed)
                + L", skipped=" + std::to_wstring(stats.skipped) + L", rollback=" + std::to_wstring(stats.rolledBack)
                + L", total us=" + std::to_wstring(stats.micros) + L", max us=" + std::to_wstring(stats.maximum));
        }
    }
}

bool start() {
    if (active) return true;
    if (target || failed) return false;
    try {
        bindSchema();
        std::wstring reason;
        auto candidate = NativeContract::resolve(reason);
        if (!candidate) { message(L"Replacement unavailable: " + reason); return false; }
        auto status = MH_Initialize();
        require(status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED, "MinHook initialization failed");
        require(MH_CreateHook(candidate, reinterpret_cast<void*>(&builder), reinterpret_cast<void**>(&original)) == MH_OK, "population builder hook creation failed");
        target = candidate; gameThread = GetCurrentThreadId();
        if (!listening.exchange(true)) FUObjectArray::AddUObjectDeleteListener(&listener);
        active = true;
        if (MH_EnableHook(target) != MH_OK) { active = false; MH_RemoveHook(target); target = nullptr; throw std::runtime_error("population builder hook activation failed"); }
        if (logging) message(L"Wolf-to-bandit prototype ready; in-game spawn/save/respawn gate pending.");
        return true;
    } catch (const std::exception& error) {
        if (warningCount.fetch_add(1) < 8) { std::string s(error.what()); message(L"Replacement unavailable: " + std::wstring(s.begin(), s.end())); }
        return false;
    }
}
void stop() {
    active = false;
    if (target) { MH_DisableHook(target); MH_RemoveHook(target); target = nullptr; }
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
        lua.register_function("_LWConfigurePrototype", [](const auto& l) {
            // LuaMadeSimple removes each consumed argument; every read is index 1.
            auto percent = l.get_integer(1); auto logs = l.get_integer(1);
            LessWildlife::enabled = LessWildlife::prototypeEnabled(percent); LessWildlife::logging = logs == 1; return 0;
        });
        lua.register_function("_LWStartPrototype", [](const auto& l) { l.set_bool(LessWildlife::start()); return 1; });
    }
    void on_lua_stop(StringViewType name, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) override {
        if (name == L"LessWildlife") LessWildlife::enabled = false;
    }
    ~LessWildlifeMod() override { LessWildlife::stop(); }
};
extern "C" __declspec(dllexport) CppUserModBase* start_mod() { return new LessWildlifeMod; }
extern "C" __declspec(dllexport) void uninstall_mod(CppUserModBase* mod) { delete mod; }
