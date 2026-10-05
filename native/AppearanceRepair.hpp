#pragma once
#include "WildlifeDefinitions.hpp"
#include <cstdint>
#include <cstring>

namespace LessWildlife {
// The engine adds an instance number to the stub's FName. The caller supplies
// its comparison name only: the original, unchanged population row identity.
constexpr bool wildlifeRow(std::wstring_view row) {
    if (row.size() < 4 || row[0] < L'0' || row[0] > L'9'
        || row[1] < L'0' || row[1] > L'9' || row[2] != L'_') return false;
    const auto index = (row[0] - L'0') * 10 + row[1] - L'0';
    if (index >= 64) return false;
    for (const auto& item : wildlifeDefinitions)
        if (row.substr(3) == item.path.substr(item.path.find_last_of(L'.') + 1)) return true;
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
