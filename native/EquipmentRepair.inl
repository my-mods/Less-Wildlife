// Runs synchronously before the normal humanoid initializer, before an actor
// exists. Read the saved inventory; let the engine seed its own equipment.
namespace EquipmentRepair {
bool hasEquipment(UObject* inventory, uint32_t member, uint8_t wantedSlot) {
    const auto count = read<int32_t>(inventory, 0x80), free = read<int32_t>(inventory, 0xac);
    require(count >= 0 && count <= 1000000 && free >= 0 && free <= count, "inventory registry bounds");
    if (count == free) return false;
    const auto size = read<uint32_t>(inventory, 0xc0);
    require(size && size <= 2097152 && !(size & (size - 1)), "inventory hash bounds");
    auto hashes = read<unsigned char*>(inventory, 0xb8);
    if (!hashes) hashes = reinterpret_cast<unsigned char*>(inventory) + 0xb0;
    auto index = read<int32_t>(hashes, (member & (size - 1)) * 4);
    auto data = read<unsigned char*>(inventory, 0x78); require(data != nullptr, "inventory registry missing");
    for (unsigned tries = 0; index != -1 && tries < 128; ++tries) {
        require(index >= 0 && index < count, "inventory registry index");
        auto pair = data + size_t(index) * 16;
        if (read<uint32_t>(pair, 0) == member) {
            const auto records = read<Array>(inventory, 0x68);
            const auto recordIndex = read<int32_t>(pair, 4);
            require(arrayValid(records, 1000000) && recordIndex >= 0 && recordIndex < records.count, "inventory record index");
            auto record = records.data + size_t(recordIndex) * 0x78;
            require(read<uint32_t>(record, 0) == member, "inventory record identity");
            const auto loadouts = read<Array>(record, 0x30);
            const auto selected = read<uint8_t>(record, 0x28);
            require(arrayValid(loadouts, 16), "inventory loadout bounds");
            if (!loadouts.count) return false;
            require(selected < loadouts.count, "inventory active loadout index");
            auto loadout = loadouts.data + size_t(selected) * 32;
            auto slots = read<Array>(loadout, 0), items = read<Array>(loadout, 16);
            require(arrayValid(slots, 64) && arrayValid(items, 64) && slots.count == items.count, "inventory equipment bounds");
            for (int i = 0; i < slots.count; ++i)
                if (slots.data[i] == wantedSlot && read<int32_t>(items.data, size_t(i) * 4) >= 0) return true;
            return false;
        }
        index = read<int32_t>(pair, 8);
    }
    require(index == -1, "inventory hash chain limit");
    return false;
}

bool prepare(UObject* definition, UObject* stub) {
    using namespace EncounterRuntime;
    if (!configurationReady || failed || (read<uint8_t>(stub, 0x58) & 0x10)) return false;
    auto record = read<unsigned char*>(stub, 0x38);
    auto entry = read<void*>(stub, 0xe8);
    auto key = entryKey(entry);
    if (!record || !key || read<void*>(stub, 0x60) != read<void*>(entry, 0x10)) return false;
    // A saved choice alone is insufficient when an area has become gated.
    auto area = findArea(*key);
    if (!area || !area->object.valid()) return false;
    const auto conditions = read<Array>(area->object.object, 0x350), entries = read<Array>(area->object.object, 0x340);
    require(arrayValid(conditions, 64) && arrayValid(entries, 64), "equipment encounter arrays");
    if (conditions.count || key->row >= entries.count || entryReason(entries.data + key->row * 0x100) != SkipReason::None) return false;
    if (read<int32_t>(stub, 0xe0) > 0 || read<int32_t>(stub, 0xe4) != 0) return false;
    Facts store(stub);
    auto saved = DecisionJournal::locate(store, *key);
    if (!saved.decision || saved.decision->phase != CyclePhase::Active) return false;
    const auto& choice = *saved.decision;
    if (choice.outcome != Outcome::Bandit && !usesHostileHumanProfile(choice.outcome)) return false;
    require(definition->GetClassPrivate()->GetPathName() == path(choice)
        && definitionKey(static_cast<unsigned char*>(read<void*>(entry, 0x10)) + 0x30) == keyFor(choice), "equipment replacement identity");
    const auto member = read<uint32_t>(stub, 0x40); require(member != 0, "equipment member identity");
    auto initialized = DecisionJournal::locate(store, *key, member);
    if (initialized.decision) {
        const auto& prior = *initialized.decision;
        require(prior.cycle <= choice.cycle, "equipment cycle moved backwards");
        if (prior.cycle == choice.cycle) {
            require(prior.outcome == choice.outcome && prior.phase == CyclePhase::Completed, "equipment choice changed within cycle");
            return false;
        }
    }
    auto& equipment = *reinterpret_cast<TMap<uint8_t, UObject*>*>(reinterpret_cast<unsigned char*>(definition) + 0x2a8);
    require(equipment.Num() >= 0 && equipment.Num() <= 8 && equipment.GetMaxIndex() <= 16, "stock equipment bounds");
    std::optional<uint8_t> slot;
    for (auto& item : equipment) if (item.Value) {
        // These exact human definitions supply one primary weapon. Reject a
        // changed multi-item default rather than duplicating existing gear.
        require(!slot, "stock equipment requires multiple items"); slot = item.Key;
    }
    require(slot.has_value(), "stock replacement weapon missing");
    struct Subsystem { UObject* context; UObject* type; UObject* result; } subsystem{stub, types[19].object, nullptr};
    types[8].object->ProcessEvent(static_cast<UFunction*>(types[9].object), &subsystem);
    require(subsystem.result && subsystem.result->IsA(static_cast<UClass*>(types[19].object)), "equipment inventory subsystem unavailable");
    const bool missing = !hasEquipment(subsystem.result, member, *slot);
    auto completed = choice; completed.phase = CyclePhase::Completed;
    // Commit before requesting seeding; failure leaves the normal initializer
    // untouched. This synchronous game-thread callback cannot save mid-write.
    // Persist even when armed, so later disarming/looting never replenishes it.
    DecisionJournal::save(store, initialized, completed);
    if (missing) {
        const auto flags = static_cast<uint16_t>(read<uint16_t>(record, 0x22) | 0x200);
        std::memcpy(record + 0x22, &flags, sizeof(flags));
    }
    return missing;
}
}
