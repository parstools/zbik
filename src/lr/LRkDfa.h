#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Identifiers.h"
#include "LRkClosure.h"

namespace zbik {

struct LRkBuildUntilConflictResult;

struct LRkState {
    ItemSet items;
    std::map<SymbolRef, StateId> transitions;
    bool operator==(const LRkState &) const = default;
};

struct LRkDfaStats {
    std::size_t states;
    std::size_t items; // Total item occurrences across all states.
    std::size_t transitions;
    bool operator==(const LRkDfaStats &) const = default;
};

// Owns the resulting graph; no references to construction helpers are retained.
class LRkDfa {
public:
    // Must return a stable hash during construction and equal hashes for equal
    // item sets. Collisions are allowed and resolved by full structural equality.
    using StateHasher = std::function<std::size_t(const ItemSet &)>;
    LRkDfa(const Grammar &grammar, std::size_t maxLength,
           StateHasher hasher = hashItems);
    LRkDfa(const Grammar &grammar, const FirstKAnalysis &first,
           StateHasher hasher = hashItems);

    // Returns a complete DFA only when no conflict was found. On conflict,
    // the partial graph remains private and only progress counters escape.
    [[nodiscard]] static LRkBuildUntilConflictResult buildUntilFirstConflict(
            const Grammar &grammar,
            const FirstKAnalysis &first,
            StateHasher hasher = hashItems);

    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] StateId start() const noexcept;
    [[nodiscard]] std::span<const LRkState> states() const noexcept;
    [[nodiscard]] const LRkState &state(StateId id) const;
    [[nodiscard]] LRkDfaStats statistics() const noexcept;
    [[nodiscard]] std::string dump() const;
    [[nodiscard]] std::string toDot() const;
    [[nodiscard]] static std::size_t hashItems(const ItemSet &items);

private:
    friend class DirectLALRkDfa;
    friend class LALRkDfa;
    friend class SelectiveLRkMerger;
    LRkDfa(Grammar grammar, std::size_t maxLength, std::vector<LRkState> states,
           std::string name);

    // Keep names and rules alive for diagnostics independently of the caller.
    Grammar grammar_;
    std::size_t maxLength_;
    std::vector<LRkState> states_;
    std::string name_;
};

struct LRkBuildUntilConflictResult {
    bool conflict;
    std::size_t completedStates;
    std::size_t discoveredStates;
    std::optional<LRkDfa> dfa;
};

} // namespace zbik
