#include "FirstK.h"

#include <stdexcept>
#include <utility>

namespace zbik {

FirstKAnalysis::FirstKAnalysis(const Grammar &grammar, std::size_t maxLength)
    : grammar_(grammar), maxLength_(maxLength) {
    first_.reserve(grammar.nonterminalCount());
    for (std::size_t i = 0; i < grammar.nonterminalCount(); ++i) {
        first_.emplace_back(maxLength);
    }

    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            changed |= first_[toIndex(rule.lhs())].unionWith(first(rule.rhs()));
        }
    } while (changed);
}

const Grammar &FirstKAnalysis::grammar() const noexcept {
    return grammar_;
}

std::size_t FirstKAnalysis::maxLength() const noexcept {
    return maxLength_;
}

const WordSetK &FirstKAnalysis::first(NonterminalId nonterminal) const {
    return first_.at(toIndex(nonterminal));
}

WordSetK FirstKAnalysis::first(const SymbolRef &symbol) const {
    if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
        if (maxLength_ == 0) {
            return WordSetK{maxLength_, {LookaheadWord{}}};
        }
        return WordSetK{maxLength_, {LookaheadWord{{*terminal}}}};
    }
    return first(std::get<NonterminalId>(symbol));
}

WordSetK FirstKAnalysis::first(std::span<const SymbolRef> symbols) const {
    WordSetK result{maxLength_, {LookaheadWord{}}};
    for (const SymbolRef &symbol: symbols) {
        result = concatenateTruncated(result, first(symbol));
        if (result.empty()) {
            break;
        }
    }
    return result;
}

WordSetK FirstKAnalysis::first(
        std::span<const SymbolRef> symbols,
        const LookaheadWord &trailingLookahead) const {
    const WordSetK trailing{maxLength_, {trailingLookahead}};
    return concatenateTruncated(first(symbols), trailing);
}

WordSetK FirstKAnalysis::firstSuffix(RuleId rule, std::size_t position) const {
    return first(suffix(rule, position));
}

WordSetK FirstKAnalysis::firstSuffix(
        RuleId rule,
        std::size_t position,
        const LookaheadWord &trailingLookahead) const {
    return first(suffix(rule, position), trailingLookahead);
}

std::span<const SymbolRef> FirstKAnalysis::suffix(RuleId rule, std::size_t position) const {
    const std::vector<SymbolRef> &rhs = grammar_.rule(rule).rhs();
    if (position > rhs.size()) {
        throw std::out_of_range("FIRST(k) suffix position exceeds rule length");
    }
    return std::span<const SymbolRef>{rhs}.subspan(position);
}

} // namespace zbik
