#pragma once

#include <span>
#include <vector>

#include "LRkDfa.h"

namespace zbik {

// A canonical LR(k) graph with states of equal LR(0) core merged.
class LALRkDfa final {
public:
    using StateHasher = LRkDfa::StateHasher;
    LALRkDfa(const Grammar &grammar, std::size_t maxLength,
             StateHasher hasher = LRkDfa::hashItems);
    explicit LALRkDfa(const LRkDfa &canonical);

    [[nodiscard]] std::size_t maxLength() const noexcept { return graph_.maxLength(); }
    [[nodiscard]] const Grammar &grammar() const noexcept { return graph_.grammar(); }
    [[nodiscard]] StateId start() const noexcept { return graph_.start(); }
    [[nodiscard]] std::span<const LRkState> states() const noexcept { return graph_.states(); }
    [[nodiscard]] const LRkState &state(StateId id) const { return graph_.state(id); }
    [[nodiscard]] LRkDfaStats statistics() const noexcept { return graph_.statistics(); }
    [[nodiscard]] std::string dump() const { return graph_.dump(); }
    [[nodiscard]] std::string toDot() const { return graph_.toDot(); }

    [[nodiscard]] const LRkDfa &canonical() const noexcept;
    [[nodiscard]] std::span<const StateId> canonicalOrigins(StateId state) const;
    [[nodiscard]] std::span<const std::vector<StateId>> originGroups() const noexcept;

private:
    friend class ParseTable;
    struct MergeResult;

    LALRkDfa(const LRkDfa &canonical, MergeResult merged);
    [[nodiscard]] static MergeResult merge(const LRkDfa &canonical);

    LRkDfa graph_;
    LRkDfa canonical_;
    std::vector<std::vector<StateId>> origins_;
};

} // namespace zbik
