#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "first/Follow1.h"
#include "first/FollowK.h"
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

TEST(FollowKAnalysisTest, PlacesEndOfInputOnlyInTheStartContextInitially) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> token",
            "Unused -> other",
    });
    const zbik::FollowKAnalysis follow(grammar, 3);

    EXPECT_EQ(
            follow.follow(nonterminal(grammar, "S")),
            zbik::WordSetK(3, {word({zbik::endOfInput})}));
    EXPECT_TRUE(follow.follow(nonterminal(grammar, "Unused")).empty());
}

TEST(FollowKAnalysisTest, CombinesSuffixLanguagesWithInheritedContexts) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> a",
            "B -> b c",
            "B -> b",
            "B ->",
    });
    const zbik::FollowKAnalysis follow(grammar, 2);

    EXPECT_EQ(
            follow.follow(nonterminal(grammar, "A")),
            zbik::WordSetK(2, {
                    word({zbik::endOfInput}),
                    word({terminal(grammar, "b"), zbik::endOfInput}),
                    word({terminal(grammar, "b"), terminal(grammar, "c")}),
            }));
    EXPECT_EQ(
            follow.follow(nonterminal(grammar, "B")),
            zbik::WordSetK(2, {word({zbik::endOfInput})}));
}

TEST(FollowKAnalysisTest, ReachesAFixedPointAcrossChainsAndCycles) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "Root -> S end tail",
            "S -> A",
            "A -> B",
            "B -> A",
            "B -> token",
    });
    const zbik::FollowKAnalysis follow(grammar, 2);
    const zbik::WordSetK expected(2, {
            word({terminal(grammar, "end"), terminal(grammar, "tail")}),
    });

    EXPECT_EQ(follow.follow(nonterminal(grammar, "S")), expected);
    EXPECT_EQ(follow.follow(nonterminal(grammar, "A")), expected);
    EXPECT_EQ(follow.follow(nonterminal(grammar, "B")), expected);
}

TEST(FollowKAnalysisTest, HandlesRepeatedNullableNonterminal) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A A",
            "A -> item",
            "A ->",
    });
    const zbik::FollowKAnalysis follow(grammar, 2);

    EXPECT_EQ(
            follow.follow(nonterminal(grammar, "A")),
            zbik::WordSetK(2, {
                    word({terminal(grammar, "item"), zbik::endOfInput}),
                    word({zbik::endOfInput}),
            }));
}

TEST(FollowKAnalysisTest, RequiresAProductiveSuffix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A marker Dead",
            "A -> value",
            "Dead -> Dead",
    });
    const zbik::FollowKAnalysis followK(grammar, 1);
    const zbik::Follow1Analysis follow1(grammar);

    EXPECT_TRUE(followK.follow(nonterminal(grammar, "A")).empty());
    EXPECT_EQ(
            follow1.follow(nonterminal(grammar, "A")).terminals.values(),
            (std::vector{terminal(grammar, "marker")}));
}

TEST(FollowKAnalysisTest, TreatsZeroLookaheadAsAProductiveContext) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> token",
            "Unused -> other",
    });
    const zbik::FollowKAnalysis follow(grammar, 0);

    EXPECT_EQ(follow.follow(nonterminal(grammar, "S")).dump(), "{[]}");
    EXPECT_EQ(follow.follow(nonterminal(grammar, "A")).dump(), "{[]}");
    EXPECT_TRUE(follow.follow(nonterminal(grammar, "Unused")).empty());
}

TEST(FollowKAnalysisTest, AgreesWithFollow1ForProductiveGrammars) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> a",
            "A ->",
            "B -> b",
            "B ->",
    });
    const zbik::Follow1Analysis follow1(grammar);
    const zbik::FollowKAnalysis followK(grammar, 1);

    for (const std::string name: {"S", "A", "B"}) {
        const zbik::NonterminalId id = nonterminal(grammar, name);
        const zbik::Follow1Set expected = follow1.follow(id);
        const zbik::WordSetK &actual = followK.follow(id);

        EXPECT_EQ(actual.contains(word({zbik::endOfInput})), expected.containsEndOfInput);
        for (const zbik::TerminalId value: expected.terminals.values()) {
            EXPECT_TRUE(actual.contains(word({value})));
        }
        EXPECT_EQ(actual.size(), expected.terminals.values().size() + expected.containsEndOfInput);
    }
}

TEST(FollowKAnalysisTest, IgnoresContextsFromUnreachableProductions) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> a",
            "Unused -> A x",
    });
    const zbik::Follow1Analysis follow1(grammar);
    const zbik::FollowKAnalysis followK(grammar, 1);
    const zbik::NonterminalId a = nonterminal(grammar, "A");

    EXPECT_EQ(followK.follow(a), zbik::WordSetK(1, {word({zbik::endOfInput})}));
    EXPECT_EQ(
            follow1.follow(a).terminals.values(),
            (std::vector{terminal(grammar, "x")}));
    EXPECT_TRUE(follow1.follow(a).containsEndOfInput);
}

TEST(FollowKAnalysisTest, KeepsContextsBehindANonproductiveLeftPrefix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> Dead A",
            "Dead -> Dead",
            "A -> a",
    });
    const zbik::FollowKAnalysis follow(grammar, 2);

    EXPECT_EQ(
            follow.follow(nonterminal(grammar, "A")),
            zbik::WordSetK(2, {word({zbik::endOfInput})}));
}

TEST(FollowKAnalysisTest, ExtendsSelfPropagatedContextsToTheLengthLimit) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> S a",
            "S -> b",
    });
    const zbik::FollowKAnalysis follow(grammar, 3);
    const zbik::TerminalId a = terminal(grammar, "a");

    EXPECT_EQ(
            follow.follow(nonterminal(grammar, "S")),
            zbik::WordSetK(3, {
                    word({zbik::endOfInput}),
                    word({a, zbik::endOfInput}),
                    word({a, a, zbik::endOfInput}),
                    word({a, a, a}),
            }));
}

TEST(FollowKAnalysisTest, IsIndependentOfRuleOrder) {
    const zbik::Grammar firstGrammar = zbik::GrammarBuilder{}.build({
            "S -> A tail",
            "A -> B",
            "B -> item",
            "B ->",
    });
    const zbik::Grammar secondGrammar = zbik::GrammarBuilder{}.build({
            "S -> A tail",
            "B ->",
            "B -> item",
            "A -> B",
    });
    const zbik::FirstKAnalysis firstA(firstGrammar, 2);
    const zbik::FirstKAnalysis firstB(secondGrammar, 2);
    const zbik::FollowKAnalysis followA(firstGrammar, firstA);
    const zbik::FollowKAnalysis followB(secondGrammar, firstB);

    for (const std::string name: {"S", "A", "B"}) {
        EXPECT_EQ(
                firstA.first(nonterminal(firstGrammar, name)).dump(),
                firstB.first(nonterminal(secondGrammar, name)).dump());
        EXPECT_EQ(
                followA.follow(nonterminal(firstGrammar, name)).dump(),
                followB.follow(nonterminal(secondGrammar, name)).dump());
    }
}

TEST(FollowKAnalysisTest, RejectsFirstResultsFromAnotherGrammar) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::Grammar other = zbik::GrammarBuilder{}.build({"S -> other"});
    const zbik::FirstKAnalysis wrongFirst(other, 2);

    EXPECT_THROW(
            static_cast<void>(zbik::FollowKAnalysis{grammar, wrongFirst}),
            std::invalid_argument);
}
