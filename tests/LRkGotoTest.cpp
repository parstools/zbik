#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>

#include "first/FirstK.h"
#include "grammar/GrammarBuilder.h"
#include "lr/LRGrammarView.h"
#include "lr/LRkClosure.h"
#include "lr/LRkGoto.h"

namespace {

zbik::LookaheadWord word(std::initializer_list<zbik::LookaheadSymbol> symbols) {
    return zbik::LookaheadWord{symbols};
}

zbik::ItemSet sorted(zbik::ItemSet items) {
    std::ranges::sort(items);
    return items;
}

} // namespace

TEST(LRkGotoTest, MovesSeparatelyOverTerminalsAndNonterminals) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::LRkGoto goTo(closure);
    const auto eof = word({zbik::endOfInput});
    const auto initial = closure.close(zbik::Item(view, view.syntheticRuleId(), 0, eof));

    EXPECT_EQ(
            goTo.goTo(initial, grammar.findTerminal("a").value()),
            (zbik::ItemSet{zbik::Item(view, zbik::RuleId{0}, 1, eof)}));
    EXPECT_EQ(
            goTo.goTo(initial, grammar.start()),
            (zbik::ItemSet{zbik::Item(view, view.syntheticRuleId(), 1, eof)}));
}

TEST(LRkGotoTest, ClosesTheShiftedKernel) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> a",
            "B -> b",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const zbik::LRkGoto goTo(closure);
    const auto eof = word({zbik::endOfInput});
    const auto initial = closure.close(zbik::Item(view, view.syntheticRuleId(), 0, eof));
    const auto b = grammar.findTerminal("b").value();

    EXPECT_EQ(
            goTo.goTo(initial, grammar.findNonterminal("A").value()),
            sorted({
                    zbik::Item(view, zbik::RuleId{0}, 1, eof),
                    zbik::Item(view, zbik::RuleId{2}, 0, eof),
            }));
    EXPECT_EQ(
            goTo.goTo(initial, grammar.findTerminal("a").value()),
            (zbik::ItemSet{
                    zbik::Item(view, zbik::RuleId{1}, 1, word({b, zbik::endOfInput}))}));
}

TEST(LRkGotoTest, PreservesEveryLookaheadWhileMovingTheDot) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const zbik::LRkGoto goTo(closure);
    const auto a = grammar.findTerminal("a").value();
    const zbik::ItemSet state{
            zbik::Item(view, zbik::RuleId{0}, 0, word({a, a})),
            zbik::Item(view, zbik::RuleId{0}, 0, word({a, zbik::endOfInput})),
    };

    EXPECT_EQ(
            goTo.goTo(state, a),
            sorted({
                    zbik::Item(view, zbik::RuleId{0}, 1, word({a, a})),
                    zbik::Item(view, zbik::RuleId{0}, 1, word({a, zbik::endOfInput})),
            }));
}

TEST(LRkGotoTest, IgnoresCompletedAndNonmatchingItems) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a b"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::LRkGoto goTo(closure);
    const auto eof = word({zbik::endOfInput});
    const zbik::ItemSet state{
            zbik::Item(view, zbik::RuleId{0}, 0, eof),
            zbik::Item(view, zbik::RuleId{0}, 2, eof),
    };

    EXPECT_TRUE(goTo.goTo(state, grammar.findTerminal("b").value()).empty());
    EXPECT_TRUE(goTo.goTo(zbik::ItemSet{}, grammar.findTerminal("a").value()).empty());
}

TEST(LRkGotoTest, RemovesDuplicateKernelItemsAndSortsTheResult) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::LRkGoto goTo(closure);
    const auto eof = word({zbik::endOfInput});
    const zbik::Item item(view, zbik::RuleId{0}, 0, eof);

    EXPECT_EQ(
            goTo.goTo(zbik::ItemSet{item, item}, grammar.findTerminal("a").value()),
            (zbik::ItemSet{zbik::Item(view, zbik::RuleId{0}, 1, eof)}));
}

TEST(LRkGotoTest, ValidatesStateBeforeReadingTheSymbolAfterDot) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const zbik::LRkGoto goTo(closure);
    const auto a = grammar.findTerminal("a").value();
    zbik::Item badDot(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));
    badDot.dot = 2;
    const zbik::Item incomplete(view, zbik::RuleId{0}, 0, word({a}));

    EXPECT_THROW(
            static_cast<void>(goTo.goTo(zbik::ItemSet{badDot}, a)),
            std::invalid_argument);
    EXPECT_THROW(
            static_cast<void>(goTo.goTo(zbik::ItemSet{incomplete}, a)),
            std::invalid_argument);
    EXPECT_THROW(
            static_cast<void>(goTo.goTo(zbik::ItemSet{}, zbik::TerminalId{99})),
            std::invalid_argument);
    EXPECT_THROW(
            static_cast<void>(goTo.goTo(zbik::ItemSet{}, zbik::NonterminalId{99})),
            std::invalid_argument);
}
