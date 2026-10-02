#include "FollowK.h"

#include <span>
#include <stdexcept>

namespace zbik {

FollowKAnalysis::FollowKAnalysis(const Grammar &grammar, std::size_t maxLength)
    : FollowKAnalysis(grammar, FirstKAnalysis{grammar, maxLength}) {}

FollowKAnalysis::FollowKAnalysis(const Grammar &grammar, const FirstKAnalysis &first)
    : grammar_(grammar), maxLength_(first.maxLength()) {
    if (&first.grammar() != &grammar) {
        throw std::invalid_argument("FollowKAnalysis requires FIRST(k) results for the same grammar");
    }

    follow_.reserve(grammar.nonterminalCount());
    for (std::size_t i = 0; i < grammar.nonterminalCount(); ++i) {
        follow_.emplace_back(maxLength_);
    }

    if (maxLength_ == 0) {
        follow_[toIndex(grammar.start())].add(LookaheadWord{});
    } else {
        follow_[toIndex(grammar.start())].add(LookaheadWord{{endOfInput}});
    }

    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            const std::vector<SymbolRef> &rhs = rule.rhs();
            for (std::size_t position = 0; position < rhs.size(); ++position) {
                const auto nonterminal = std::get_if<NonterminalId>(&rhs[position]);
                if (nonterminal == nullptr) {
                    continue;
                }

                const std::span<const SymbolRef> suffix{rhs.begin() + position + 1, rhs.end()};
                WordSetK contexts{maxLength_};
                for (const LookaheadWord &lookahead: follow_[toIndex(rule.lhs())].words()) {
                    contexts.unionWith(first.first(suffix, lookahead));
                }
                changed |= follow_[toIndex(*nonterminal)].unionWith(contexts);
            }
        }
    } while (changed);
}

const Grammar &FollowKAnalysis::grammar() const noexcept {
    return grammar_;
}

std::size_t FollowKAnalysis::maxLength() const noexcept {
    return maxLength_;
}

const WordSetK &FollowKAnalysis::follow(NonterminalId nonterminal) const {
    return follow_.at(toIndex(nonterminal));
}

} // namespace zbik
