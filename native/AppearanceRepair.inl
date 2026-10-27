// Saved appearance IDs are shared by animals and humanoids, including fixed
// monster appearances. Reinitialize each owned member once per saved cycle.
namespace AppearanceRepair {
bool prepare(UObject* definition, UObject* stub) {
    using namespace EncounterRuntime;
    if (!configurationReady || failed || (read<uint8_t>(stub, 0x58) & 0x08)) return false;
    auto record = read<unsigned char*>(stub, 0x38);
    auto entry = read<void*>(stub, 0xe8);
    auto key = entryKey(entry);
    if (!record || !key || read<void*>(stub, 0x60) != read<void*>(entry, 0x10)) return false;
    auto area = findArea(*key);
    if (!area || !area->object.valid()) return false;
    auto conditions = read<Array>(area->object.object, 0x350);
    auto entries = read<Array>(area->object.object, 0x340);
    require(arrayValid(conditions, 64) && arrayValid(entries, 64), "appearance encounter arrays");
    if (conditions.count || key->row >= entries.count
        || entryReason(entries.data + key->row * 0x100) != SkipReason::None) return false;
    if (read<int32_t>(stub, 0xe0) > 0 || read<int32_t>(stub, 0xe4) != 0) return false;
    Facts store(stub);
    auto saved = DecisionJournal::locate(store, *key);
    if (!saved.decision || saved.decision->phase != CyclePhase::Active) return false;
    const auto& choice = *saved.decision;
    if (choice.outcome == Outcome::Original || choice.outcome == Outcome::None) return false;
    require(definition->GetClassPrivate()->GetPathName() == path(choice)
        && definitionKey(static_cast<unsigned char*>(read<void*>(entry, 0x10)) + 0x30) == keyFor(choice),
        "appearance replacement identity");
    auto member = read<uint32_t>(stub, 0x40);
    require(member != 0, "appearance member identity");
    auto initialized = DecisionJournal::locate(store, *key, member, true);
    if (initialized.decision) {
        const auto& prior = *initialized.decision;
        require(prior.cycle <= choice.cycle, "appearance cycle moved backwards");
        if (prior.cycle == choice.cycle) {
            require(prior.outcome == choice.outcome && prior.phase == CyclePhase::Completed,
                "appearance choice changed within cycle");
            return false;
        }
    }
    ScopedObjectRef owner(stub), definitionOwner(definition);
    // Streaming can restore members outside an attempt callback.
    prepareRoamingPoints(choice);
    const auto previous = read<std::array<unsigned char, 3>>(record, 0x24);
    auto completed = choice; completed.phase = CyclePhase::Completed;
    DecisionJournal::save(store, initialized, completed);
    require(active && owner.valid() && definitionOwner.valid()
        && read<void*>(stub, 0x38) == record
        && read<std::array<unsigned char, 3>>(record, 0x24) == previous,
        "appearance initializer storage changed");
    // The normal initializer selects the target's own fixed or random preset.
    std::memset(record + 0x24, 0, 3);
    return true;
}
}
