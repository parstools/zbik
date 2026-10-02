#include "Nullable.h"

namespace zbik {

NullableAnalysis::NullableAnalysis(const Grammar &grammar)
    : grammar_(grammar), nullable_(grammar.nonterminalCount(), false) {
    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            const std::size_t lhs = toIndex(rule.lhs());
            if (!nullable_[lhs] && isNullable(rule.rhs())) {
                nullable_[lhs] = true;
                changed = true;
            }
        }
    } while (changed);
}

const Grammar &NullableAnalysis::grammar() const noexcept {
    return grammar_;
}

bool NullableAnalysis::isNullable(NonterminalId nonterminal) const {
    return nullable_.at(toIndex(nonterminal));
}

bool NullableAnalysis::isNullable(const SymbolRef &symbol) const {
    if (std::holds_alternative<TerminalId>(symbol)) {
        return false;
    }
    return isNullable(std::get<NonterminalId>(symbol));
}

bool NullableAnalysis::isNullable(std::span<const SymbolRef> symbols) const {
    for (const SymbolRef &symbol: symbols) {
        if (!isNullable(symbol)) {
            return false;
        }
    }
    return true;
}

} // namespace zbik
