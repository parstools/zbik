#pragma once

#include <compare>
#include <cstddef>

#include "LRGrammarView.h"
#include "first/LookaheadWord.h"
#include "grammar/Identifiers.h"

namespace zbik {

struct ItemCore {
    RuleId rule;
    std::size_t dot;

    auto operator<=>(const ItemCore &) const = default;
};

struct Item {
    RuleId rule;
    std::size_t dot;
    LookaheadWord lookahead;

    Item(
            const LRGrammarView &grammar,
            RuleId rule,
            std::size_t dot,
            LookaheadWord lookahead);

    [[nodiscard]] ItemCore core() const noexcept;

    auto operator<=>(const Item &) const = default;
};

} // namespace zbik
