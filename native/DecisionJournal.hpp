#pragma once
#include "EncounterCycle.hpp"
#include <string>
#include <optional>
#include <bit>

namespace LessWildlife {
// Two-bank records live in the game's saved integer-fact database. A head
// switch commits a fully verified bank. An interrupted update retains the old
// bank; unknown occupied names are never adopted or overwritten.
class DecisionJournal {
    static constexpr std::uint32_t headMagic = 0x4c574a20;
    static constexpr unsigned probes = 8, words = 10;
    static std::wstring prefix(const EncounterKey& key, unsigned probe) {
        constexpr wchar_t hex[] = L"0123456789abcdef";
        std::wstring result = L"mod.lesswildlife.v1.";
        for (auto word : key.area) for (unsigned n = 0; n < 8; ++n) result += hex[(word >> (n * 4)) & 15];
        result += L"." + std::to_wstring(key.row) + L"." + std::to_wstring(key.definition) + L"." + std::to_wstring(probe);
        return result;
    }
    static std::wstring name(const std::wstring& root, unsigned index) { return root + L".w" + std::to_wstring(index); }
    template<class Store> static std::optional<EncounterDecision> bank(Store& store, const std::wstring& root, unsigned index) {
        DecisionBytes bytes{};
        for (unsigned i = 0; i < words; ++i) {
            auto v = store.get(name(root, index * words + i));
            if (!v) return {};
            const auto word = std::bit_cast<std::uint32_t>(*v);
            for (unsigned b = 0; b < 4; ++b) bytes[i * 4 + b] = static_cast<std::uint8_t>(word >> (b * 8));
        }
        return decodeDecision(bytes);
    }
public:
    struct Slot { std::wstring root; std::optional<EncounterDecision> decision; unsigned active{}; };
    template<class Store> static Slot locate(Store& store, const EncounterKey& key) {
        if (!valid(key)) throw std::invalid_argument("journal encounter identity");
        std::optional<Slot> vacant;
        for (unsigned probe = 0; probe < probes; ++probe) {
            auto root = prefix(key, probe);
            // Store checks the actual engine's hashed keys, including pairwise
            // collisions between this record's own names.
            if (!store.distinct(root, 21)) continue;
            auto head = store.get(name(root, 20));
            if (head && (std::bit_cast<std::uint32_t>(*head) & ~1u) == headMagic) {
                const unsigned index = std::bit_cast<std::uint32_t>(*head) & 1;
                auto saved = bank(store, root, index);
                if (!saved) throw std::runtime_error("committed encounter decision is damaged");
                if (saved->key == key) return {std::move(root), saved, index};
                continue;
            }
            bool empty = !head;
            for (unsigned i = 0; empty && i < 20; ++i) empty = !store.get(name(root, i));
            if (empty && !vacant) vacant = Slot{std::move(root), {}, 1};
        }
        if (vacant) return std::move(*vacant);
        throw std::runtime_error("all encounter journal slots are occupied or invalid");
    }
    template<class Store> static void save(Store& store, Slot& slot, const EncounterDecision& next) {
        if (!valid(next) || (slot.decision && slot.decision->key != next.key)) throw std::invalid_argument("journal decision identity");
        auto current = locate(store, next.key);
        if (current.root != slot.root || current.decision != slot.decision) throw std::runtime_error("encounter journal changed during update");
        const auto bytes = encodeDecision(next);
        const unsigned target = slot.active ^ 1;
        for (unsigned i = 0; i < words; ++i) {
            std::uint32_t word{};
            for (unsigned b = 0; b < 4; ++b) word |= std::uint32_t(bytes[i * 4 + b]) << (8 * b);
            store.set(name(slot.root, target * words + i), std::bit_cast<std::int32_t>(word));
            // Reserve both banks on creation. Leaving the inactive names
            // absent would let a later record claim a colliding hashed key.
            if (!slot.decision) store.set(name(slot.root, (target ^ 1) * words + i), std::bit_cast<std::int32_t>(word));
        }
        if (bank(store, slot.root, target) != std::optional{next}) throw std::runtime_error("encounter decision readback failed");
        if (!slot.decision && bank(store, slot.root, target ^ 1) != std::optional{next})
            throw std::runtime_error("encounter journal reservation failed");
        store.set(name(slot.root, 20), std::bit_cast<std::int32_t>(headMagic | target));
        if (store.get(name(slot.root, 20)) != std::optional{std::bit_cast<std::int32_t>(headMagic | target)})
            throw std::runtime_error("encounter journal commit failed");
        slot.decision = next; slot.active = target;
    }
};
}
