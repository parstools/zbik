#pragma once

#include <string>
#include <vector>

#include "ParseTable.h"

namespace zbik {

enum class SelectiveMergeMode { ExactActions, CompatibleUnion };

struct SelectiveMergeOptions {
    SelectiveMergeMode mode = SelectiveMergeMode::CompatibleUnion;
    std::size_t maxAttempts = 10000;
};

struct SelectiveMergeStats {
    bool usedLalr = false;
    bool retainedCanonical = false;
    std::size_t lalrStates = 0;
    std::size_t lalrConflicts = 0;
    std::size_t attempts = 0;
    std::size_t committed = 0;
    std::size_t rejected = 0;
    bool budgetExhausted = false;
};

struct SelectiveMergeResult {
    LRkDfa graph;
    std::vector<StateId> canonicalToMerged;
    std::vector<std::vector<StateId>> origins;
    SelectiveMergeStats statistics;
};

// CompatibleUnion first uses conflict-free LALR(k), then selective merging.
// A selective candidate is used only if it has strictly fewer states.
// Experimental construction for conflict-free canonical LR(k). The result
// owns its graph and does not require the canonical graph at runtime.
class SelectiveLRkMerger {
public:
    [[nodiscard]] static SelectiveMergeResult merge(
            const LRkDfa &canonical, SelectiveMergeOptions options = {});

    // Independently verifies a certificate against the canonical graph.
    // Returns an empty string on success, a diagnostic otherwise.
    [[nodiscard]] static std::string validate(
            const LRkDfa &canonical, const SelectiveMergeResult &result,
            SelectiveMergeMode mode);
};

} // namespace zbik
