#include "First1.h"

#include <stdexcept>
#include <utility>

namespace zbik {

First1Analysis::First1Analysis(const Grammar &grammar)
    : First1Analysis(grammar, NullableAnalysis{grammar}) {}

First1Analysis::First1Analysis(const Grammar &grammar, const NullableAnalysis &nullable)
    : grammar_(grammar),
      nullable_(nullable),
      first_(grammar.nonterminalCount(), TerminalSet{grammar.terminalCount()}) {
    if (&nullable.grammar() != &grammar) {
        throw std::invalid_argument("First1Analysis requires nullable results for the same grammar");
    }

    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar_.rules()) {
            const First1Set rhsFirst = first(rule.rhs());
            changed |= first_[toIndex(rule.lhs())].unionWith(rhsFirst.terminals);
        }
    } while (changed);
}

const Grammar &First1Analysis::grammar() const noexcept {
    return grammar_;
}

const NullableAnalysis &First1Analysis::nullable() const noexcept {
    return nullable_;
}

First1Set First1Analysis::first(NonterminalId nonterminal) const {
    return {
            first_.at(toIndex(nonterminal)),
            nullable_.isNullable(nonterminal),
    };
}

First1Set First1Analysis::first(const SymbolRef &symbol) const {
    if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
        TerminalSet terminals{grammar_.terminalCount()};
        terminals.add(*terminal);
        return {std::move(terminals), false};
    }
    return first(std::get<NonterminalId>(symbol));
}

First1Set First1Analysis::first(std::span<const SymbolRef> symbols) const {
    TerminalSet terminals{grammar_.terminalCount()};
    for (const SymbolRef &symbol: symbols) {
        const First1Set symbolFirst = first(symbol);
        terminals.unionWith(symbolFirst.terminals);
        if (!symbolFirst.containsEmptyWord) {
            return {std::move(terminals), false};
        }
    }
    return {std::move(terminals), true};
}

} // namespace zbik
