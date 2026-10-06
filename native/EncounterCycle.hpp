#pragma once
#include "ReplacementPolicy.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace LessWildlife {
// Engine-independent decision rules. The runtime adapter must supply an
// original area GUID and source row index, and save the returned record in the
// same game save before applying it. These rules alone do not provide storage
// or establish that an engine respawn attempt represents a new pack cycle.
struct EncounterKey {
    std::array<std::uint32_t, 4> area{};
    // Definition codes 0..9 are the stock list order in WildlifeDefinitions.hpp
    // when codec version 1 was introduced. Reordering requires a format migration.
    std::uint8_t row{}, definition{};
    bool operator==(const EncounterKey&) const = default;
};
enum class CyclePhase : std::uint8_t { Active, SuppressionPending, Completed };
struct EncounterDecision {
    EncounterKey key;
    std::uint64_t cycle{};
    Outcome outcome{Outcome::Original};
    CyclePhase phase{CyclePhase::Active};
    bool operator==(const EncounterDecision&) const = default;
};

inline bool valid(const EncounterKey& key) {
    return (key.area[0] || key.area[1] || key.area[2] || key.area[3]) && key.row < 64 && key.definition < 10;
}
inline bool valid(const EncounterDecision& decision) {
    return valid(decision.key) && static_cast<unsigned>(decision.outcome) <= static_cast<unsigned>(Outcome::None)
        && static_cast<unsigned>(decision.phase) <= static_cast<unsigned>(CyclePhase::Completed)
        && (decision.outcome == Outcome::None
            ? decision.phase != CyclePhase::Active : decision.phase != CyclePhase::SuppressionPending);
}

template<class Uniform>
EncounterDecision beginCycle(const EncounterKey& key, std::uint64_t cycle, const Options& options, Uniform&& uniform) {
    if (!valid(key)) throw std::invalid_argument("encounter identity");
    const auto outcome = choose(options, uniform);
    return {key, cycle, outcome, outcome == Outcome::None ? CyclePhase::SuppressionPending : CyclePhase::Active};
}

// Travel, repeated overlap and restoration always reuse the saved record.
// Changed settings, including an empty pool, never rewrite an existing cycle.
template<class Uniform>
EncounterDecision restoreOrBegin(const EncounterKey& key, const std::optional<EncounterDecision>& saved,
    const Options& options, Uniform&& uniform) {
    if (!valid(key)) throw std::invalid_argument("encounter identity");
    if (!saved) return beginCycle(key, 0, options, uniform);
    if (!valid(*saved) || !(saved->key == key)) throw std::invalid_argument("saved encounter identity/state");
    return *saved;
}

// Completion requires the normal game's acknowledgement for this exact cycle.
// Partial kills and an old callback cannot complete a different/current group.
inline bool acknowledgeCompletion(EncounterDecision& decision, std::uint64_t expectedCycle,
    bool engineCompleted, unsigned survivingMembers) {
    if (!valid(decision) || decision.cycle != expectedCycle || !engineCompleted || survivingMembers) return false;
    decision.phase = CyclePhase::Completed;
    return true;
}

enum class RespawnCause { Restore, PartialRefill, Forced, Natural };
// A respawn callback returning true is insufficient. The adapter must prove a
// completed group is naturally eligible, with no survivors or pending corpses.
template<class Uniform>
bool advanceCycle(EncounterDecision& decision, std::uint64_t expectedCycle, RespawnCause cause,
    bool engineEligible, unsigned remainingMembers, const Options& options, Uniform&& uniform) {
    if (!valid(decision) || decision.cycle != expectedCycle || decision.phase != CyclePhase::Completed
        || cause != RespawnCause::Natural || !engineEligible || remainingMembers) return false;
    if (decision.cycle == std::numeric_limits<std::uint64_t>::max()) return false;
    auto next = beginCycle(decision.key, decision.cycle + 1, options, uniform);
    decision = next;
    return true;
}

// A versioned value format used by DecisionJournal. Never use native struct
// padding, object pointers or FName indices as persistent data. The checksum
// detects damaged records; it is not an authentication or collision guarantee.
using DecisionBytes = std::array<std::uint8_t, 40>;
inline std::uint32_t decisionChecksum(const DecisionBytes& bytes) {
    std::uint32_t hash = 2166136261u;
    for (size_t i = 0; i < 36; ++i) { hash ^= bytes[i]; hash *= 16777619u; }
    return hash;
}
inline DecisionBytes encodeDecision(const EncounterDecision& decision) {
    if (!valid(decision)) throw std::invalid_argument("invalid encounter decision");
    DecisionBytes bytes{};
    auto put = [&](size_t offset, std::uint64_t value, size_t width) {
        for (size_t i = 0; i < width; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    };
    bytes[0] = 'L'; bytes[1] = 'W'; bytes[2] = 'C'; bytes[3] = 1;
    for (size_t i = 0; i < 4; ++i) put(4 + i * 4, decision.key.area[i], 4);
    bytes[20] = decision.key.row; bytes[21] = decision.key.definition;
    bytes[22] = static_cast<std::uint8_t>(decision.outcome); bytes[23] = static_cast<std::uint8_t>(decision.phase);
    put(24, decision.cycle, 8);
    put(36, decisionChecksum(bytes), 4);
    return bytes;
}
inline std::optional<EncounterDecision> decodeDecision(const DecisionBytes& bytes) {
    auto get = [&](size_t offset, size_t width) {
        std::uint64_t value = 0;
        for (size_t i = 0; i < width; ++i) value |= std::uint64_t(bytes[offset + i]) << (i * 8);
        return value;
    };
    if (bytes[0] != 'L' || bytes[1] != 'W' || bytes[2] != 'C' || bytes[3] != 1
        || get(32, 4) != 0 || get(36, 4) != decisionChecksum(bytes)) return std::nullopt;
    EncounterDecision result;
    for (size_t i = 0; i < 4; ++i) result.key.area[i] = static_cast<std::uint32_t>(get(4 + i * 4, 4));
    result.key.row = bytes[20]; result.key.definition = bytes[21];
    result.outcome = static_cast<Outcome>(bytes[22]); result.phase = static_cast<CyclePhase>(bytes[23]);
    result.cycle = get(24, 8);
    return valid(result) ? std::optional{result} : std::nullopt;
}
}
