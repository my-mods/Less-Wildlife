#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace LessWildlife {
struct RowIdentity {
    std::uint8_t row{}, definition{};
    bool operator==(const RowIdentity&) const = default;
};

// Names and the exact wildlife catalog are immutable within a binding. Cache
// only their parsed scalar identities, including non-wildlife names. Decisions,
// area GUIDs and borrowed engine objects must always come from the current call.
class RowIdentityCache {
    struct Entry {
        std::uint64_t name{};
        std::optional<RowIdentity> identity;
        bool occupied{};
    };
    std::array<Entry, 1024> entries{};
public:
    void clear() { entries = {}; }
    template<class Parse> std::optional<RowIdentity> get(std::uint64_t name, Parse&& parse) {
        auto mixed = (name ^ (name >> 32)) * 0x9e3779b97f4a7c15ull;
        auto& entry = entries[mixed >> 54];
        if (entry.occupied && entry.name == name) return entry.identity;
        auto identity = parse();
        // Publish only a completed parse. Collisions replace a single slot;
        // the complete FName (comparison index and number) must still match.
        entry = {name, identity, true};
        return identity;
    }
};
}
