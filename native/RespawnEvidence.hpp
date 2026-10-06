#pragma once
#include <array>
#include <cstdint>

namespace LessWildlife {
// Observations are not persisted decisions. In particular, an accepted refill
// must never be used as evidence that an entire pack started a new cycle.
struct RespawnSnapshot {
    std::array<std::uint32_t, 4> area{};
    std::uint64_t row{};
    int phase{}, living{}, dead{}, recycled{};
    bool operator==(const RespawnSnapshot&) const = default;
};
enum class RespawnEvent : unsigned { Queued, Eligibility, Attempt };
enum class RespawnEvidence {
    NextDayQueued, Eligible, VisibilityBlocked, PopulationUnavailable,
    InactivePhase, CleanupRetry, Deferred, ForcedAccepted, RefillAccepted,
    EmptyGroupAccepted
};
inline RespawnEvidence classifyAttempt(const RespawnSnapshot& sample, bool forced, bool accepted) {
    if (sample.phase < 0) return RespawnEvidence::InactivePhase;
    if (!accepted) return !forced && sample.dead > 0 ? RespawnEvidence::CleanupRetry : RespawnEvidence::Deferred;
    if (forced) return RespawnEvidence::ForcedAccepted;
    return sample.living > 0 ? RespawnEvidence::RefillAccepted : RespawnEvidence::EmptyGroupAccepted;
}

// A fixed-size, session-only diagnostic cache. No pointers, saved decisions or
// game writes. Continuous identical eligibility checks produce no repeated log.
class RespawnLogCache {
    struct Slot {
        RespawnSnapshot sample{};
        RespawnEvidence evidence{};
        RespawnEvent event{};
        std::uint64_t sampledAt{};
        bool used{}, reported{};
    };
    std::array<Slot, 128> slots{};
    unsigned next{};
public:
    void clear() { slots = {}; next = 0; }
    bool reserve(const RespawnSnapshot& identity, RespawnEvent event, std::uint64_t now) {
        for (auto& slot : slots) {
            if (!slot.used || slot.sample.area != identity.area || slot.sample.row != identity.row || slot.event != event) continue;
            if (event == RespawnEvent::Eligibility && now - slot.sampledAt < 1000) return false;
            slot.sampledAt = now;
            return true;
        }
        auto& slot = slots[next++ % slots.size()];
        slot = {}; slot.sample = identity; slot.event = event; slot.sampledAt = now; slot.used = true;
        return true;
    }
    bool changed(const RespawnSnapshot& sample, RespawnEvent event, RespawnEvidence evidence) {
        for (auto& slot : slots) {
            if (!slot.used || slot.sample.area != sample.area || slot.sample.row != sample.row || slot.event != event) continue;
            const bool changed = !slot.reported || slot.sample != sample || slot.evidence != evidence;
            slot.sample = sample; slot.evidence = evidence; slot.reported = true;
            return changed;
        }
        return false;
    }
};
}
