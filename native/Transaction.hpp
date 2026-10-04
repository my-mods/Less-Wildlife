#pragma once
#include <cstddef>

namespace LessWildlife {
enum class TransactionResult { Applied, RolledBack, RollbackFailed };

// Operations and their snapshots are prepared before any write. Include the
// failing operation in rollback: a setter can fail after partially changing it.
template<class Operations>
TransactionResult commit(Operations& operations) noexcept {
    std::size_t attempted = 0;
    try {
        for (auto& operation : operations) {
            ++attempted;
            operation.apply();
            if (!operation.readbackMatches()) throw 0;
        }
        return TransactionResult::Applied;
    } catch (...) {
        bool restored = true;
        while (attempted) {
            auto& operation = operations[--attempted];
            try { operation.restore(); if (!operation.restored()) restored = false; }
            catch (...) { restored = false; }
        }
        return restored ? TransactionResult::RolledBack : TransactionResult::RollbackFailed;
    }
}
}
