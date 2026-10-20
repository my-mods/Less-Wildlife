#pragma once
#include "WildlifeDefinitions.hpp"
#include <cstdint>
#include <cstring>

namespace LessWildlife {
// The engine adds an instance number to the stub's FName. The caller supplies
// its comparison name only: the original, unchanged population row identity.
constexpr bool wildlifeRow(std::wstring_view row) {
    const auto separator = row.find(L'_');
    if (separator == std::wstring_view::npos || separator == 0 || separator > 2) return false;
    unsigned index = 0;
    for (size_t i = 0; i < separator; ++i) {
        if (row[i] < L'0' || row[i] > L'9') return false;
        index = index * 10 + row[i] - L'0';
    }
    if (index >= 64) return false;
    for (const auto& item : wildlifeDefinitions)
        if (row.substr(separator + 1) == item.path.substr(item.path.find_last_of(L'.') + 1)) return true;
    return false;
}

// Quadrupeds store their coat variant in this word; humanoids store a saved
// appearance ID. The following byte is the humanoid table row (one-based).
// Retain initialized human appearances, including travel/load restoration.
// Required native readers/writers are checked before this operation is used.
inline bool clearAnimalAppearance(void* record) {
    if (!record) return false;
    auto bytes = static_cast<unsigned char*>(record);
    uint16_t appearance{};
    std::memcpy(&appearance, bytes + 0x24, sizeof(appearance));
    if (!appearance || bytes[0x26] != 0) return false;
    appearance = 0;
    std::memcpy(bytes + 0x24, &appearance, sizeof(appearance));
    return true;
}
}
