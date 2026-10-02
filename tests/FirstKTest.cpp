#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "first/First1.h"
#include "first/FirstK.h"
#include "grammar/GrammarBuilder.h"

namespace {

zbik::NonterminalId nonterminal(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findNonterminal(name).value();
}

zbik::TerminalId terminal(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findTerminal(name).value();
}

zbik::LookaheadWord word(std::initializer_list<zbik::LookaheadSymbol> symbols) {
    return zbik::LookaheadWord{symbols};
}

} // namespace

TEST(FirstKAnalysisTest, ComputesTerminalPrefixesUpToTheRequestedLength) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> a",
            "A ->",
            "B -> b c",
            "B -> b",
    });
    const zbik::FirstKAnalysis first(grammar, 2);

    EXPECT_EQ(first.first(nonterminal(grammar, "A")).dump(), "{[] [t0]}");
    EXPECT_EQ(first.first(nonterminal(grammar, "B")).dump(), "{[t1] [t1 t2]}");
    EXPECT_EQ(first.first(nonterminal(grammar, "S")).dump(), "{[t0 t1] [t1] [t1 t2]}");
}

TEST(FirstKAnalysisTest, ReachesAFixedPointThroughLeftRecursionAndCycles) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> A plus",
            "A -> B",
            "B -> A",
            "B -> term",
    });
    const zbik::FirstKAnalysis first(grammar, 3);
    const std::string expected = "{[t1] [t1 t0] [t1 t0 t0]}";

    EXPECT_EQ(first.first(nonterminal(grammar, "S")).dump(), expected);
    EXPECT_EQ(first.first(nonterminal(grammar, "A")).dump(), expected);
    EXPECT_EQ(first.first(nonterminal(grammar, "B")).dump(), expected);
}

TEST(FirstKAnalysisTest, RequiresTheWholeSequenceToBeProductive) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> a B",
            "B -> B",
    });
    const zbik::FirstKAnalysis firstK(grammar, 1);
    const zbik::First1Analysis first1(grammar);

    EXPECT_TRUE(firstK.first(nonterminal(grammar, "S")).empty());
    EXPECT_TRUE(firstK.first(nonterminal(grammar, "B")).empty());
    EXPECT_EQ(
            first1.first(nonterminal(grammar, "S")).terminals.values(),
            (std::vector{terminal(grammar, "a")}));
}

TEST(FirstKAnalysisTest, TreatsZeroLookaheadAsProductivity) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> token",
            "Empty ->",
            "Dead -> Dead",
    });
    const zbik::FirstKAnalysis first(grammar, 0);

    EXPECT_EQ(first.first(nonterminal(grammar, "S")).dump(), "{[]}");
    EXPECT_EQ(first.first(nonterminal(grammar, "Empty")).dump(), "{[]}");
    EXPECT_TRUE(first.first(nonterminal(grammar, "Dead")).empty());
}

TEST(FirstKAnalysisTest, ComputesRuleSuffixesWithAnInheritedLookahead) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> a B c",
            "B -> b",
    });
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::Rule &rule = grammar.rule(zbik::RuleId{0});

    EXPECT_EQ(
            first.firstSuffix(rule.id(), 1),
            zbik::WordSetK(2, {word({terminal(grammar, "b"), terminal(grammar, "c")})}));
    EXPECT_EQ(first.firstSuffix(rule.id(), rule.size()), zbik::WordSetK(2, {word({})}));
    EXPECT_EQ(
            first.firstSuffix(rule.id(), 2, word({zbik::endOfInput})),
            zbik::WordSetK(2, {word({terminal(grammar, "c"), zbik::endOfInput})}));
    EXPECT_THROW(
            static_cast<void>(first.firstSuffix(rule.id(), rule.size() + 1)),
            std::out_of_range);
}

TEST(FirstKAnalysisTest, CountsEndOfInputTowardTheLengthLimit) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::Rule &rule = grammar.rule(zbik::RuleId{0});
    const zbik::FirstKAnalysis first1(grammar, 1);
    const zbik::FirstKAnalysis first2(grammar, 2);

    EXPECT_EQ(first1.firstSuffix(rule.id(), 0, word({zbik::endOfInput})).dump(), "{[t0]}");
    EXPECT_EQ(first2.firstSuffix(rule.id(), 0, word({zbik::endOfInput})).dump(), "{[t0 $]}");
    EXPECT_EQ(first2.firstSuffix(rule.id(), 1, word({zbik::endOfInput})).dump(), "{[$]}");
    EXPECT_THROW(
            static_cast<void>(
                    zbik::FirstKAnalysis(grammar, 0).firstSuffix(
                            rule.id(), 1, word({zbik::endOfInput}))),
            std::invalid_argument);
}

TEST(FirstKAnalysisTest, AgreesWithFirst1ForProductiveGrammars) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> a",
            "A ->",
            "B -> b",
    });
    const zbik::First1Analysis first1(grammar);
    const zbik::FirstKAnalysis firstK(grammar, 1);

    for (const std::string name: {"S", "A", "B"}) {
        const zbik::NonterminalId id = nonterminal(grammar, name);
        const zbik::First1Set expected = first1.first(id);
        const zbik::WordSetK &actual = firstK.first(id);

        EXPECT_EQ(actual.contains(word({})), expected.containsEmptyWord);
        for (const zbik::TerminalId value: expected.terminals.values()) {
            EXPECT_TRUE(actual.contains(word({value})));
        }
        EXPECT_EQ(actual.size(), expected.terminals.values().size() + expected.containsEmptyWord);
    }
}
