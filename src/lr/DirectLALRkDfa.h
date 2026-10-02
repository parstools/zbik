#pragma once

#include <span>

#include "LRkDfa.h"

namespace zbik {

// Builds LALR(k) lookaheads directly on the LR(0)-core graph. Unlike
// LALRkDfa, this construction never materializes the canonical LR(k) graph
// and therefore cannot attribute a conflict specifically to core merging.
class DirectLALRkDfa final {
public:
    DirectLALRkDfa(const Grammar &grammar, std::size_t maxLength);
    DirectLALRkDfa(const Grammar &grammar, const FirstKAnalysis &first);

    [[nodiscard]] std::size_t maxLength() const noexcept { return graph_.maxLength(); }
    [[nodiscard]] const Grammar &grammar() const noexcept { return graph_.grammar(); }
    [[nodiscard]] StateId start() const noexcept { return graph_.start(); }
    [[nodiscard]] std::span<const LRkState> states() const noexcept { return graph_.states(); }
    [[nodiscard]] const LRkState &state(StateId id) const { return graph_.state(id); }
    [[nodiscard]] LRkDfaStats statistics() const noexcept { return graph_.statistics(); }
    [[nodiscard]] std::string dump() const { return graph_.dump(); }
    [[nodiscard]] std::string toDot() const { return graph_.toDot(); }

private:
    friend class ParseTable;
    [[nodiscard]] static LRkDfa build(
            const Grammar &grammar, const FirstKAnalysis &first);
    LRkDfa graph_;
};

} // namespace zbik
