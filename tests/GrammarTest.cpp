#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "grammar/GrammarBuilder.h"

static_assert(std::is_same_v<
              decltype(std::declval<zbik::Grammar &>().rules()),
              const std::vector<zbik::Rule> &>);
static_assert(std::is_same_v<
              decltype(std::declval<zbik::Rule &>().rhs()),
              const std::vector<zbik::SymbolRef> &>);

TEST(GrammarTest, BuildsGrammarFromRules) {
    const std::vector<std::string> rules{
            "S -> C C",
            "C -> e C",
            "C -> d",
    };

    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build(rules);

    EXPECT_EQ(grammar.ruleCount(), 3U);
    EXPECT_EQ(grammar.nonterminalCount(), 2U);
    EXPECT_EQ(grammar.terminalCount(), 2U);
    EXPECT_EQ(grammar.rule(zbik::RuleId{0}).id(), zbik::RuleId{0});
    EXPECT_EQ(grammar.rule(zbik::RuleId{1}).id(), zbik::RuleId{1});
    EXPECT_EQ(grammar.rule(zbik::RuleId{2}).id(), zbik::RuleId{2});
    EXPECT_EQ(grammar.start(), zbik::NonterminalId{0});
    EXPECT_EQ(grammar.nonterminalName(zbik::NonterminalId{0}), "S");
    EXPECT_EQ(grammar.terminalName(zbik::TerminalId{0}), "e");
}

TEST(GrammarTest, DumpsGrammarDeterministically) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> C C",
            "C -> e C",
            "C ->",
    });

    EXPECT_EQ(
            grammar.dump(),
            "start: N0 S\n"
            "nonterminals:\n"
            "  N0: S\n"
            "  N1: C\n"
            "terminals:\n"
            "  T0: e\n"
            "rules:\n"
            "  R0: S -> C C\n"
            "  R1: C -> e C\n"
            "  R2: C -> <epsilon>\n");

    EXPECT_EQ(grammar.dump(), grammar.dump());
}

TEST(GrammarTest, AcceptsLeftRecursionAndCyclesAsGrammarData) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> S a",
            "S -> A",
            "A -> S",
            "A ->",
    });

    ASSERT_EQ(grammar.ruleCount(), 4U);
    EXPECT_EQ(
            grammar.rule(zbik::RuleId{0}).rhs(),
            (std::vector<zbik::SymbolRef>{zbik::NonterminalId{0}, zbik::TerminalId{0}}));
    EXPECT_EQ(
            grammar.rule(zbik::RuleId{1}).rhs(),
            (std::vector<zbik::SymbolRef>{zbik::NonterminalId{1}}));
    EXPECT_EQ(
            grammar.rule(zbik::RuleId{2}).rhs(),
            (std::vector<zbik::SymbolRef>{zbik::NonterminalId{0}}));
    EXPECT_TRUE(grammar.rule(zbik::RuleId{3}).rhs().empty());
}

TEST(GrammarTest, KeepsIdsAndDumpStableAcrossIndependentBuilds) {
    const std::vector<std::string> source{
            "Start -> Later token",
            "Later -> token",
            "Later ->",
    };

    const zbik::Grammar first = zbik::GrammarBuilder{}.build(source);
    const zbik::Grammar second = zbik::GrammarBuilder{}.build(source);

    EXPECT_EQ(first.start(), second.start());
    EXPECT_EQ(first.findNonterminal("Start"), second.findNonterminal("Start"));
    EXPECT_EQ(first.findNonterminal("Later"), second.findNonterminal("Later"));
    EXPECT_EQ(first.findTerminal("token"), second.findTerminal("token"));
    ASSERT_EQ(first.ruleCount(), second.ruleCount());
    for (std::size_t i = 0; i < first.ruleCount(); ++i) {
        const zbik::RuleId id{static_cast<std::uint32_t>(i)};
        EXPECT_EQ(first.rule(id).id(), second.rule(id).id());
        EXPECT_EQ(first.rule(id).lhs(), second.rule(id).lhs());
        EXPECT_EQ(first.rule(id).rhs(), second.rule(id).rhs());
    }
    EXPECT_EQ(first.dump(), second.dump());
}
