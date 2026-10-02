#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "first/FirstK.h"
#include "grammar/GrammarBuilder.h"
#include "lr/LRGrammarView.h"
#include "lr/LRkClosure.h"

namespace {

zbik::LookaheadWord word(std::initializer_list<zbik::LookaheadSymbol> symbols) {
    return zbik::LookaheadWord{symbols};
}

zbik::ItemSet sorted(zbik::ItemSet items) {
    std::ranges::sort(items);
    return items;
}

} // namespace

TEST(LRkClosureTest, ExpandsTheSyntheticStartTransitively) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> token",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::LookaheadWord eof = word({zbik::endOfInput});
    const zbik::Item seed(view, view.syntheticRuleId(), 0, eof);

    EXPECT_EQ(
            closure.close(seed),
            sorted({
                    zbik::Item(view, view.syntheticRuleId(), 0, eof),
                    zbik::Item(view, zbik::RuleId{0}, 0, eof),
                    zbik::Item(view, zbik::RuleId{1}, 0, eof),
            }));
}

TEST(LRkClosureTest, StopsWhenTheDotPrecedesATerminalOrCompletesARule) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::LookaheadWord eof = word({zbik::endOfInput});
    const zbik::Item beforeTerminal(view, zbik::RuleId{0}, 0, eof);
    const zbik::Item completed(view, zbik::RuleId{0}, 1, eof);

    EXPECT_EQ(closure.close(beforeTerminal), (zbik::ItemSet{beforeTerminal}));
    EXPECT_EQ(closure.close(completed), (zbik::ItemSet{completed}));
}

TEST(LRkClosureTest, UsesFirstOfTheFullSuffixAndInheritedLookahead) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A x y",
            "A -> a",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const zbik::Item seed(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));

    EXPECT_EQ(
            closure.close(seed),
            sorted({
                    seed,
                    zbik::Item(
                            view,
                            zbik::RuleId{1},
                            0,
                            word({
                                    grammar.findTerminal("x").value(),
                                    grammar.findTerminal("y").value(),
                            })),
            }));
}

TEST(LRkClosureTest, ProducesEveryLookaheadFromANullableSuffix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> a",
            "B -> b",
            "B ->",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const zbik::LookaheadWord eof = word({zbik::endOfInput});
    const zbik::Item seed(view, zbik::RuleId{0}, 0, eof);
    const zbik::TerminalId b = grammar.findTerminal("b").value();

    EXPECT_EQ(
            closure.close(seed),
            sorted({
                    seed,
                    zbik::Item(view, zbik::RuleId{1}, 0, eof),
                    zbik::Item(view, zbik::RuleId{1}, 0, word({b, zbik::endOfInput})),
            }));
}

TEST(LRkClosureTest, TerminatesAcrossCyclesAndRemovesDuplicates) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> B",
            "B -> A",
            "B -> token",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::LookaheadWord eof = word({zbik::endOfInput});
    const zbik::Item seed(view, zbik::RuleId{0}, 0, eof);
    const std::vector duplicateSeeds{seed, seed};

    EXPECT_EQ(
            closure.close(duplicateSeeds),
            sorted({
                    seed,
                    zbik::Item(view, zbik::RuleId{1}, 0, eof),
                    zbik::Item(view, zbik::RuleId{2}, 0, eof),
                    zbik::Item(view, zbik::RuleId{3}, 0, eof),
            }));
}

TEST(LRkClosureTest, AddsNothingThroughANonproductiveSuffix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A marker Dead",
            "A -> value",
            "Dead -> Dead",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    const zbik::Item seed(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));

    EXPECT_EQ(closure.close(seed), (zbik::ItemSet{seed}));
}

TEST(LRkClosureTest, SupportsZeroLookaheadForProductiveSuffixes) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A token",
            "A -> value",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 0);
    const zbik::LRkClosure closure(view, first);
    const zbik::LookaheadWord epsilon;
    const zbik::Item seed(view, zbik::RuleId{0}, 0, epsilon);

    EXPECT_EQ(
            closure.close(seed),
            sorted({seed, zbik::Item(view, zbik::RuleId{1}, 0, epsilon)}));
}

TEST(LRkClosureTest, PropagatesNewLookaheadsThroughTheSameCore) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> S a", "S -> b"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const zbik::Item seed(view, view.syntheticRuleId(), 0, word({zbik::endOfInput}));
    const auto a = grammar.findTerminal("a").value();
    zbik::ItemSet expected{seed};
    for (const auto rule: {zbik::RuleId{0}, zbik::RuleId{1}}) {
        for (const auto &lookahead: {word({zbik::endOfInput}),
                                    word({a, zbik::endOfInput}), word({a, a})}) {
            expected.emplace_back(view, rule, 0, lookahead);
        }
    }
    EXPECT_EQ(closure.close(seed), sorted(expected));
}

TEST(LRkClosureTest, ClosesAnEmptySetToAnEmptySet) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkClosure closure(view, first);
    EXPECT_TRUE(closure.close(zbik::ItemSet{}).empty());
}

TEST(LRkClosureTest, IsIdempotentAndIndependentOfSeedOrder) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> B", "B -> A", "B -> token",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const auto token = grammar.findTerminal("token").value();
    const zbik::Item eofSeed(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));
    const zbik::Item tokenSeed(view, zbik::RuleId{1}, 0, word({token, zbik::endOfInput}));
    const auto result = closure.close(zbik::ItemSet{eofSeed, tokenSeed});

    EXPECT_EQ(result.size(), 7U);
    EXPECT_EQ(closure.close(result), result);
    EXPECT_EQ(closure.close(zbik::ItemSet{tokenSeed, eofSeed, tokenSeed}), result);
}

TEST(LRkClosureTest, AddsACompletedItemForAnEmptyProduction) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> A", "A ->"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkClosure closure(view, first);
    const auto eof = word({zbik::endOfInput});
    const zbik::Item seed(view, view.syntheticRuleId(), 0, eof);
    EXPECT_EQ(closure.close(seed), sorted({
            seed, zbik::Item(view, zbik::RuleId{0}, 0, eof),
            zbik::Item(view, zbik::RuleId{1}, 0, eof),
    }));
}

TEST(LRkClosureTest, ZeroLookaheadStillRequiresAProductiveSuffix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A Dead", "A -> a", "Dead -> Dead",
    });
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 0);
    const zbik::LRkClosure closure(view, first);
    const zbik::Item seed(view, zbik::RuleId{0}, 0, word({}));
    EXPECT_EQ(closure.close(seed), (zbik::ItemSet{seed}));
}

TEST(LRkClosureTest, RejectsIncompatibleFirstAndMalformedLookaheads) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::Grammar other = zbik::GrammarBuilder{}.build({"S -> other"});
    const zbik::LRGrammarView view(grammar);
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::FirstKAnalysis wrongFirst(other, 2);
    const zbik::LRkClosure closure(view, first);

    EXPECT_THROW(
            static_cast<void>(zbik::LRkClosure(view, wrongFirst)),
            std::invalid_argument);

    const zbik::Item incomplete(view, zbik::RuleId{0}, 0, word({zbik::TerminalId{0}}));
    const zbik::Item tooLong(
            view,
            zbik::RuleId{0},
            0,
            word({zbik::TerminalId{0}, zbik::TerminalId{0}, zbik::TerminalId{0}}));
    const zbik::Item unknown(
            view,
            zbik::RuleId{0},
            0,
            word({zbik::TerminalId{99}, zbik::endOfInput}));
    zbik::Item mutatedDot(view, zbik::RuleId{0}, 0, word({zbik::endOfInput}));
    mutatedDot.dot = 2;

    EXPECT_THROW(static_cast<void>(closure.close(incomplete)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(closure.close(tooLong)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(closure.close(unknown)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(closure.close(mutatedDot)), std::invalid_argument);
}
