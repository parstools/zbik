#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lr/Item.h"

namespace {

zbik::LookaheadWord word(std::initializer_list<zbik::LookaheadSymbol> symbols) {
    return zbik::LookaheadWord{symbols};
}

} // namespace

TEST(ItemTest, RepresentsRuleDotAndLookaheadStructurally) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::LRGrammarView view(grammar);
    const zbik::Item item(view, zbik::RuleId{0}, 1, word({zbik::endOfInput}));

    EXPECT_EQ(item.rule, zbik::RuleId{0});
    EXPECT_EQ(item.dot, 1U);
    EXPECT_EQ(item.lookahead, word({zbik::endOfInput}));
    EXPECT_EQ(item.core(), (zbik::ItemCore{zbik::RuleId{0}, 1}));
}

TEST(ItemTest, KeepsLookaheadInFullIdentityButNotInTheCore) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::LRGrammarView view(grammar);
    const zbik::Item first(view, zbik::RuleId{0}, 0, word({zbik::TerminalId{0}}));
    const zbik::Item same(view, zbik::RuleId{0}, 0, word({zbik::TerminalId{0}}));
    const zbik::Item differentLookahead(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));

    EXPECT_EQ(first, same);
    EXPECT_NE(first, differentLookahead);
    EXPECT_EQ(first.core(), differentLookahead.core());
}

TEST(ItemTest, DistinguishesTextuallyIdenticalRulesByRuleId) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> token",
            "S -> token",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::Item first(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));
    const zbik::Item second(view, zbik::RuleId{1}, 0, word({zbik::endOfInput}));

    EXPECT_NE(first, second);
    EXPECT_NE(first.core(), second.core());
}

TEST(ItemTest, HasDeterministicStructuralOrdering) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> first",
            "S -> second",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::Item ruleOne(view, zbik::RuleId{1}, 0, word({zbik::endOfInput}));
    const zbik::Item laterDot(view, zbik::RuleId{0}, 1, word({zbik::endOfInput}));
    const zbik::Item laterLookahead(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));
    const zbik::Item firstItem(view, zbik::RuleId{0}, 0, word({zbik::TerminalId{0}}));
    std::vector items{ruleOne, laterDot, laterLookahead, firstItem};

    std::ranges::sort(items);

    EXPECT_EQ(items, (std::vector{firstItem, laterLookahead, laterDot, ruleOne}));
}

TEST(ItemTest, ValidatesDotForUserAndSyntheticRules) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::LRGrammarView view(grammar);

    EXPECT_NO_THROW(zbik::Item(view, zbik::RuleId{0}, 1, word({zbik::endOfInput})));
    EXPECT_NO_THROW(zbik::Item(view, view.syntheticRuleId(), 1, word({zbik::endOfInput})));
    EXPECT_THROW(
            zbik::Item(view, zbik::RuleId{0}, 2, word({zbik::endOfInput})),
            std::invalid_argument);
    EXPECT_THROW(
            zbik::Item(view, view.syntheticRuleId(), 2, word({zbik::endOfInput})),
            std::invalid_argument);
}
