#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace LessWildlife {
// Borrowed scalar targets exist only inside the synchronous engine callback.
struct CountEdit {
    std::array<void*,3> targets{};
    std::array<std::uint32_t,3> before{}, after{};
    CountEdit() = default;
    CountEdit(void* phase, void* quantity, void* maximum, std::array<std::uint32_t,3> values)
        : targets{phase,quantity,maximum}, after(values) {
        for (unsigned i=0;i<3;++i) std::memcpy(&before[i],targets[i],4);
    }
    void apply() const { for(unsigned i=0;i<3;++i) if(targets[i])std::memcpy(targets[i],&after[i],4); }
    void restore() const { for(unsigned i=0;i<3;++i) if(targets[i])std::memcpy(targets[i],&before[i],4); }
    bool matches(const std::array<std::uint32_t,3>& values) const {
        for(unsigned i=0;i<3;++i) if(targets[i] && std::memcmp(targets[i],&values[i],4)) return false;
        return true;
    }
    bool readbackMatches() const { return matches(after); }
    bool restored() const { return matches(before); }
};
struct RowCounts {
    bool known{}, capacityReady{};
    std::array<std::uint32_t,3> original{}, owned{};
};
}
