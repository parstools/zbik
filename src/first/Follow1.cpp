#include "Follow1.h"

#include <span>
#include <stdexcept>

namespace zbik {

Follow1Analysis::Follow1Analysis(const Grammar &grammar) : Follow1Analysis(grammar, First1Analysis{grammar}) {
}

Follow1Analysis::Follow1Analysis(const Grammar &grammar, const First1Analysis &first) :
    grammar_(grammar), follow_(grammar.nonterminalCount(), Follow1Set{TerminalSet{grammar.terminalCount()}, false}) {
    if (&first.grammar() != &grammar) {
        throw std::invalid_argument("Follow1Analysis requires FIRST(1) results for the same grammar");
    }

    follow_[toIndex(grammar.start())].containsEndOfInput = true;

    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            const auto &rhs = rule.rhs();
            for (std::size_t position = 0; position < rhs.size(); ++position) {
                const auto nonterminal = std::get_if<NonterminalId>(&rhs[position]);
                if (nonterminal == nullptr) {
                    continue;
                }

                const std::span<const SymbolRef> suffix{rhs.begin() + position + 1, rhs.end()};
                const First1Set suffixFirst = first.first(suffix);
                Follow1Set &target = follow_[toIndex(*nonterminal)];
                changed |= target.terminals.unionWith(suffixFirst.terminals);

                if (suffixFirst.containsEmptyWord) {
                    const Follow1Set &lhsFollow = follow_[toIndex(rule.lhs())];
                    changed |= target.terminals.unionWith(lhsFollow.terminals);
                    if (lhsFollow.containsEndOfInput && !target.containsEndOfInput) {
                        target.containsEndOfInput = true;
                        changed = true;
                    }
                }
            }
        }
    } while (changed);
}

const Grammar &Follow1Analysis::grammar() const noexcept {
    return grammar_;
}

Follow1Set Follow1Analysis::follow(NonterminalId nonterminal) const {
    return follow_.at(toIndex(nonterminal));
}

} // namespace zbik
