#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "first/First1.h"
#include "first/Nullable.h"
#include "grammar/GrammarBuilder.h"

namespace {

zbik::NonterminalId nonterminal(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findNonterminal(name).value();
}

zbik::TerminalId terminal(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findTerminal(name).value();
}

} // namespace

TEST(NullableAnalysisTest, FindsNullableChainsByFixedPoint) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A ->",
            "B -> C",
            "C ->",
            "N -> token",
    });
    const zbik::NullableAnalysis nullable(grammar);

    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "S")));
    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "A")));
    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "B")));
    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "C")));
    EXPECT_FALSE(nullable.isNullable(nonterminal(grammar, "N")));
    EXPECT_TRUE(nullable.isNullable(std::vector<zbik::SymbolRef>{}));
}

TEST(NullableAnalysisTest, TerminatesOnNullableCycle) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> B",
            "B -> A",
            "B ->",
    });
    const zbik::NullableAnalysis nullable(grammar);

    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "S")));
    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "A")));
    EXPECT_TRUE(nullable.isNullable(nonterminal(grammar, "B")));
}

TEST(First1AnalysisTest, HandlesLeftRecursion) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "E -> E plus",
            "E -> term",
    });
    const zbik::First1Analysis first(grammar);
    const zbik::First1Set result = first.first(nonterminal(grammar, "E"));

    EXPECT_EQ(result.terminals.values(), (std::vector{terminal(grammar, "term")}));
    EXPECT_FALSE(result.containsEmptyWord);
}

TEST(First1AnalysisTest, ReachesFixedPointAcrossCycles) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> B",
            "B -> A",
            "B -> token",
    });
    const zbik::First1Analysis first(grammar);
    const auto expected = std::vector{terminal(grammar, "token")};

    EXPECT_EQ(first.first(nonterminal(grammar, "S")).terminals.values(), expected);
    EXPECT_EQ(first.first(nonterminal(grammar, "A")).terminals.values(), expected);
    EXPECT_EQ(first.first(nonterminal(grammar, "B")).terminals.values(), expected);
}

TEST(First1AnalysisTest, ComputesFirstOfSymbolsAndSequences) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B tail",
            "A ->",
            "B -> first",
    });
    const zbik::NullableAnalysis nullable(grammar);
    const zbik::First1Analysis first(grammar, nullable);
    const zbik::Rule &startRule = grammar.rule(zbik::RuleId{0});

    const zbik::First1Set sequenceFirst = first.first(startRule.rhs());
    EXPECT_EQ(sequenceFirst.terminals.values(), (std::vector{terminal(grammar, "first")}));
    EXPECT_FALSE(sequenceFirst.containsEmptyWord);

    const zbik::First1Set terminalFirst = first.first(zbik::SymbolRef{terminal(grammar, "tail")});
    EXPECT_EQ(terminalFirst.terminals.values(), (std::vector{terminal(grammar, "tail")}));
    EXPECT_FALSE(terminalFirst.containsEmptyWord);

    const zbik::First1Set emptyFirst = first.first(std::vector<zbik::SymbolRef>{});
    EXPECT_TRUE(emptyFirst.terminals.empty());
    EXPECT_TRUE(emptyFirst.containsEmptyWord);
}

TEST(First1AnalysisTest, IncludesTerminalsAfterNullablePrefix) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A B",
            "A -> first",
            "A ->",
            "B -> second",
    });
    const zbik::First1Analysis first(grammar);
    const zbik::First1Set result = first.first(nonterminal(grammar, "S"));

    EXPECT_EQ(
            result.terminals.values(),
            (std::vector{terminal(grammar, "first"), terminal(grammar, "second")}));
    EXPECT_FALSE(result.containsEmptyWord);
}

TEST(First1AnalysisTest, RejectsNullableResultsFromAnotherGrammar) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    const zbik::Grammar other = zbik::GrammarBuilder{}.build({"S ->"});
    const zbik::NullableAnalysis wrongNullable(other);

    EXPECT_THROW(
            static_cast<void>(zbik::First1Analysis{grammar, wrongNullable}),
            std::invalid_argument);
}
