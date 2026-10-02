#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "WordSetK.h"
#include "grammar/Grammar.h"

namespace zbik {

class FirstKAnalysis {
public:
    FirstKAnalysis(const Grammar &grammar, std::size_t maxLength);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] const WordSetK &first(NonterminalId nonterminal) const;
    [[nodiscard]] WordSetK first(const SymbolRef &symbol) const;
    [[nodiscard]] WordSetK first(std::span<const SymbolRef> symbols) const;
    [[nodiscard]] WordSetK first(
            std::span<const SymbolRef> symbols,
            const LookaheadWord &trailingLookahead) const;
    [[nodiscard]] WordSetK firstSuffix(RuleId rule, std::size_t position) const;
    [[nodiscard]] WordSetK firstSuffix(
            RuleId rule,
            std::size_t position,
            const LookaheadWord &trailingLookahead) const;

private:
    [[nodiscard]] std::span<const SymbolRef> suffix(RuleId rule, std::size_t position) const;

    const Grammar &grammar_;
    std::size_t maxLength_;
    std::vector<WordSetK> first_;
};

} // namespace zbik
