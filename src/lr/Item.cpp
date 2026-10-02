#include "Item.h"

#include <stdexcept>
#include <utility>

namespace zbik {

Item::Item(
        const LRGrammarView &grammar,
        RuleId rule,
        std::size_t dot,
        LookaheadWord lookahead)
    : rule(rule), dot(dot), lookahead(std::move(lookahead)) {
    if (dot > grammar.rule(rule).size()) {
        throw std::invalid_argument("item dot position exceeds rule length");
    }
}

ItemCore Item::core() const noexcept {
    return {rule, dot};
}

} // namespace zbik
