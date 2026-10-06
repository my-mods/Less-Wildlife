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
    static std::wstring prefix(const EncounterKey& key, unsigned probe, std::uint32_t member) {
        constexpr wchar_t hex[] = L"0123456789abcdef";
        std::wstring result = member ? L"mod.lesswildlife.equipment.v1." : L"mod.lesswildlife.v1.";
        for (auto word : key.area) for (unsigned n = 0; n < 8; ++n) result += hex[(word >> (n * 4)) & 15];
        result += L"." + std::to_wstring(key.row) + L"." + std::to_wstring(key.definition) + L"." + std::to_wstring(probe);
        if (member) result += L"." + std::to_wstring(member);
        return result;
    }
    static std::wstring name(const std::wstring& root, unsigned index) { return root + L".w" + std::to_wstring(index); }
    struct Record {
        EncounterDecision decision;
        std::uint32_t member{};
        bool operator==(const Record&) const = default;
    };
    static void checksum(DecisionBytes& bytes) {
        auto value = decisionChecksum(bytes);
        for (unsigned i = 0; i < 4; ++i) bytes[36 + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
    template<class Store> static std::optional<Record> bank(Store& store, const std::wstring& root, unsigned index, bool memberRecord) {
        DecisionBytes bytes{};
        for (unsigned i = 0; i < words; ++i) {
            auto v = store.get(name(root, index * words + i));
            if (!v) return {};
            const auto word = std::bit_cast<std::uint32_t>(*v);
            for (unsigned b = 0; b < 4; ++b) bytes[i * 4 + b] = static_cast<std::uint8_t>(word >> (b * 8));
        }
        std::uint32_t member{}, check{};
        for (unsigned i = 0; i < 4; ++i) {
            member |= std::uint32_t(bytes[32 + i]) << (i * 8);
            check |= std::uint32_t(bytes[36 + i]) << (i * 8);
        }
        if (check != decisionChecksum(bytes) || (member != 0) != memberRecord) return {};
        // Member records use the reserved word in their separate namespace.
        // Keep ordinary encounter records and their v1 codec byte-identical.
        for (unsigned i = 32; i < 36; ++i) bytes[i] = 0;
        checksum(bytes);
        auto decision = decodeDecision(bytes);
        return decision ? std::optional{Record{*decision, member}} : std::nullopt;
    }
public:
    struct Slot { std::wstring root; std::optional<EncounterDecision> decision; unsigned active{}; std::uint32_t member{}; };
    template<class Store> static Slot locate(Store& store, const EncounterKey& key, std::uint32_t member = 0) {
        if (!valid(key)) throw std::invalid_argument("journal encounter identity");
        std::optional<Slot> vacant;
        for (unsigned probe = 0; probe < probes; ++probe) {
            auto root = prefix(key, probe, member);
            // Store checks the actual engine's hashed keys, including pairwise
            // collisions between this record's own names.
            if (!store.distinct(root, 21)) continue;
            auto head = store.get(name(root, 20));
            if (head && (std::bit_cast<std::uint32_t>(*head) & ~1u) == headMagic) {
                const unsigned index = std::bit_cast<std::uint32_t>(*head) & 1;
                auto saved = bank(store, root, index, member != 0);
                if (!saved) throw std::runtime_error("committed encounter decision is damaged");
                if (saved->decision.key == key && saved->member == member) return {std::move(root), saved->decision, index, member};
                continue;
            }
            bool empty = !head;
            for (unsigned i = 0; empty && i < 20; ++i) empty = !store.get(name(root, i));
            if (empty && !vacant) vacant = Slot{std::move(root), {}, 1, member};
        }
        if (vacant) return std::move(*vacant);
        throw std::runtime_error("all encounter journal slots are occupied or invalid");
    }
    template<class Store> static void save(Store& store, Slot& slot, const EncounterDecision& next) {
        if (!valid(next) || (slot.decision && slot.decision->key != next.key)) throw std::invalid_argument("journal decision identity");
        auto current = locate(store, next.key, slot.member);
        if (current.root != slot.root || current.decision != slot.decision) throw std::runtime_error("encounter journal changed during update");
        auto bytes = encodeDecision(next);
        for (unsigned i = 0; i < 4; ++i) bytes[32 + i] = static_cast<std::uint8_t>(slot.member >> (i * 8));
        checksum(bytes);
        const unsigned target = slot.active ^ 1;
        for (unsigned i = 0; i < words; ++i) {
            std::uint32_t word{};
            for (unsigned b = 0; b < 4; ++b) word |= std::uint32_t(bytes[i * 4 + b]) << (8 * b);
            store.set(name(slot.root, target * words + i), std::bit_cast<std::int32_t>(word));
            // Reserve both banks on creation. Leaving the inactive names
            // absent would let a later record claim a colliding hashed key.
            if (!slot.decision) store.set(name(slot.root, (target ^ 1) * words + i), std::bit_cast<std::int32_t>(word));
        }
        const Record expected{next, slot.member};
        if (bank(store, slot.root, target, slot.member != 0) != std::optional{expected}) throw std::runtime_error("encounter decision readback failed");
        if (!slot.decision && bank(store, slot.root, target ^ 1, slot.member != 0) != std::optional{expected})
            throw std::runtime_error("encounter journal reservation failed");
        store.set(name(slot.root, 20), std::bit_cast<std::int32_t>(headMagic | target));
        if (store.get(name(slot.root, 20)) != std::optional{std::bit_cast<std::int32_t>(headMagic | target)})
            throw std::runtime_error("encounter journal commit failed");
        slot.decision = next; slot.active = target;
    }
};
}
