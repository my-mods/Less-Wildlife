#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace LessWildlife {
// Saved IDs and pool bits are append-only: No spawn must remain ID 5 / bit 4.
enum class Outcome : std::uint8_t { Original = 0, Bandit = 1, BloodGuard = 2, Vidmo = 3, Kobold = 4, None = 5, Guard = 6 };
constexpr bool usesHostileHumanProfile(Outcome outcome) {
    return outcome == Outcome::BloodGuard || outcome == Outcome::Guard;
}
struct Options {
    unsigned chance = 0;
    std::array<bool, 6> allowed{true, true, true, true, false, true};
};

// The caller owns the encounter's saved decision and respawn cycle. This
// function must never be called merely because an area overlaps or streams in.
template<class Uniform>
Outcome choose(const Options& options, Uniform&& uniform) {
    if (options.chance > 100) throw std::invalid_argument("replacement chance");
    std::array<Outcome, 6> pool{};
    unsigned count = 0;
    for (unsigned i = 0; i != pool.size(); ++i)
        if (options.allowed[i]) pool[count++] = static_cast<Outcome>(i + 1);
    if (!count || !options.chance) return Outcome::Original;
    auto draw = [&](unsigned bound) {
        const auto value = uniform(bound);
        if (value < 0 || static_cast<std::uint64_t>(value) >= bound) throw std::out_of_range("replacement random draw");
        return static_cast<unsigned>(value);
    };
    if (options.chance != 100 && draw(100) >= options.chance) return Outcome::Original;
    return pool[count == 1 ? 0 : draw(count)];
}

}
