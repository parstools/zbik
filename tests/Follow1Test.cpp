#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "first/Follow1.h"
#include "grammar/GrammarBuilder.h"

namespace {

zbik::NonterminalId nonterminal(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findNonterminal(name).value();
}

zbik::TerminalId terminal(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findTerminal(name).value();
}

} // namespace

TEST(Follow1AnalysisTest, PlacesEndOfInputOnlyInStartFollowInitially) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> token",
            "Unused -> other",
    });
    const zbik::Follow1Analysis follow(grammar);

    const zbik::Follow1Set startFollow = follow.follow(nonterminal(grammar, "S"));
    EXPECT_TRUE(startFollow.terminals.empty());
    EXPECT_TRUE(startFollow.containsEndOfInput);

    const zbik::Follow1Set unusedFollow = follow.follow(nonterminal(grammar, "Unused"));
    EXPECT_TRUE(unusedFollow.terminals.empty());
    EXPECT_FALSE(unusedFollow.containsEndOfInput);
}

TEST(Follow1AnalysisTest, AddsFirstOfSuffixAndPropagatesThroughNullableSuffix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> first",
            "A ->",
            "B -> second",
            "B ->",
    });
    const zbik::Follow1Analysis follow(grammar);

    const zbik::Follow1Set aFollow = follow.follow(nonterminal(grammar, "A"));
    EXPECT_EQ(aFollow.terminals.values(), (std::vector{terminal(grammar, "second")}));
    EXPECT_TRUE(aFollow.containsEndOfInput);

    const zbik::Follow1Set bFollow = follow.follow(nonterminal(grammar, "B"));
    EXPECT_TRUE(bFollow.terminals.empty());
    EXPECT_TRUE(bFollow.containsEndOfInput);
}

TEST(Follow1AnalysisTest, ReachesFixedPointAcrossChainsAndCycles) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> B",
            "B -> C",
            "C -> A",
            "C -> token",
    });
    const zbik::Follow1Analysis follow(grammar);

    EXPECT_TRUE(follow.follow(nonterminal(grammar, "S")).containsEndOfInput);
    EXPECT_TRUE(follow.follow(nonterminal(grammar, "A")).containsEndOfInput);
    EXPECT_TRUE(follow.follow(nonterminal(grammar, "B")).containsEndOfInput);
    EXPECT_TRUE(follow.follow(nonterminal(grammar, "C")).containsEndOfInput);
}

TEST(Follow1AnalysisTest, PropagatesTerminalFollowAcrossChainsAndCycles) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "Root -> S end",
            "S -> A",
            "A -> B",
            "B -> A",
            "B -> token",
    });
    const zbik::Follow1Analysis follow(grammar);
    const auto expected = std::vector{terminal(grammar, "end")};

    EXPECT_EQ(follow.follow(nonterminal(grammar, "S")).terminals.values(), expected);
    EXPECT_EQ(follow.follow(nonterminal(grammar, "A")).terminals.values(), expected);
    EXPECT_EQ(follow.follow(nonterminal(grammar, "B")).terminals.values(), expected);
}

TEST(Follow1AnalysisTest, HandlesRepeatedNullableNonterminal) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A A",
            "A -> item",
            "A ->",
    });
    const zbik::Follow1Analysis follow(grammar);
    const zbik::Follow1Set result = follow.follow(nonterminal(grammar, "A"));

    EXPECT_EQ(result.terminals.values(), (std::vector{terminal(grammar, "item")}));
    EXPECT_TRUE(result.containsEndOfInput);
}

TEST(Follow1AnalysisTest, DistinguishesUserTerminalFromEndOfInput) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A EOF",
            "A ->",
    });
    const zbik::Follow1Analysis follow(grammar);

    const zbik::Follow1Set aFollow = follow.follow(nonterminal(grammar, "A"));
    EXPECT_EQ(aFollow.terminals.values(), (std::vector{terminal(grammar, "EOF")}));
    EXPECT_FALSE(aFollow.containsEndOfInput);
    EXPECT_TRUE(follow.follow(nonterminal(grammar, "S")).containsEndOfInput);
}

TEST(Follow1AnalysisTest, RejectsFirstResultsFromAnotherGrammar) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::Grammar other = zbik::GrammarBuilder{}.build({"S -> other"});
    const zbik::First1Analysis wrongFirst(other);

    EXPECT_THROW(static_cast<void>(zbik::Follow1Analysis{grammar, wrongFirst}), std::invalid_argument);
}
