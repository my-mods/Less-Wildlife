#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
namespace LessWildlife::MemoryAccess {
// Only this leaf copy catches bad reads. Engine callbacks and writes are never
// enclosed in SEH: swallowing their faults could hide a partial mutation.
inline int readFault(EXCEPTION_POINTERS* info) noexcept {
    const auto& e = *info->ExceptionRecord;
    return (e.ExceptionCode == EXCEPTION_ACCESS_VIOLATION || e.ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
        && e.NumberParameters >= 2 && e.ExceptionInformation[0] == 0
        ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
__declspec(noinline) inline bool copy(void* destination, const void* source, size_t size) noexcept {
    __try { std::memcpy(destination, source, size); return true; }
    __except(readFault(GetExceptionInformation())) { return false; }
}
inline const void* offset(const void* base, size_t displacement, size_t size) noexcept {
    const auto address = reinterpret_cast<uintptr_t>(base);
    const auto maximum = (std::numeric_limits<uintptr_t>::max)();
    if (!address || displacement > maximum-address || size > maximum-(address+displacement)) return nullptr;
    return reinterpret_cast<const void*>(address+displacement);
}
}
