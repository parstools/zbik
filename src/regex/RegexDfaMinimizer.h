#pragma once

#include <span>
#include <vector>

#include "regex/RegexDfa.h"

namespace zbik {

struct MinimizedDfaState {
    bool accepting;
    std::vector<DfaByteTransition> byteTransitions;
    std::vector<DfaStateId> originalStates;

    friend bool operator==(const MinimizedDfaState &,
                           const MinimizedDfaState &) = default;
};

class MinimizedRegexDfa {
public:
    [[nodiscard]] DfaStateId startState() const noexcept;
    [[nodiscard]] std::span<const MinimizedDfaState> states() const noexcept;
    [[nodiscard]] const MinimizedDfaState &state(DfaStateId id) const;

private:
    friend class RegexDfaMinimizer;
    explicit MinimizedRegexDfa(std::vector<MinimizedDfaState> states);

    std::vector<MinimizedDfaState> states_;
};

class RegexDfaMinimizer {
public:
    [[nodiscard]] static MinimizedRegexDfa minimize(const RegexDfa &dfa);
};

} // namespace zbik
