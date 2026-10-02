#include <gtest/gtest.h>

#include <string>

#include "grammar/GrammarBuilder.h"

namespace {

void expectBuildError(
        const std::vector<std::string> &lines,
        std::size_t expectedLine,
        const std::string &expectedMessage) {
    try {
        static_cast<void>(zbik::GrammarBuilder{}.build(lines));
        FAIL() << "Expected GrammarBuildError";
    } catch (const zbik::GrammarBuildError &error) {
        EXPECT_EQ(error.line(), expectedLine);
        EXPECT_NE(std::string{error.what()}.find(expectedMessage), std::string::npos);
    }
}

} // namespace

TEST(GrammarBuilderTest, ResolvesNonterminalDefinedLater) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> Later token",
            "Later -> value",
    });

    ASSERT_EQ(grammar.ruleCount(), 2U);
    const zbik::Rule &firstRule = grammar.rule(zbik::RuleId{0});
    ASSERT_EQ(firstRule.rhs().size(), 2U);
    EXPECT_EQ(firstRule.rhs()[0], zbik::SymbolRef{zbik::NonterminalId{1}});
    EXPECT_EQ(firstRule.rhs()[1], zbik::SymbolRef{zbik::TerminalId{0}});
}

TEST(GrammarBuilderTest, UsesEmptyRightHandSideAsEpsilon) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S ->",
            "S -> epsilon",
    });

    ASSERT_EQ(grammar.ruleCount(), 2U);
    EXPECT_TRUE(grammar.rule(zbik::RuleId{0}).rhs().empty());
    ASSERT_EQ(grammar.rule(zbik::RuleId{1}).rhs().size(), 1U);
    EXPECT_EQ(grammar.symbolName(grammar.rule(zbik::RuleId{1}).rhs()[0]), "epsilon");
}

TEST(GrammarBuilderTest, IgnoresOnlyFullLineComments) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "  ; grammar comment",
            "",
            "S -> ;",
    });

    ASSERT_EQ(grammar.ruleCount(), 1U);
    ASSERT_EQ(grammar.terminalCount(), 1U);
    EXPECT_EQ(grammar.terminalName(zbik::TerminalId{0}), ";");
}

TEST(GrammarBuilderTest, PreservesDuplicateProductions) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> token",
            "S -> token",
    });

    ASSERT_EQ(grammar.ruleCount(), 2U);
    EXPECT_EQ(grammar.rule(zbik::RuleId{0}).rhs(), grammar.rule(zbik::RuleId{1}).rhs());
    EXPECT_NE(grammar.rule(zbik::RuleId{0}).id(), grammar.rule(zbik::RuleId{1}).id());
    EXPECT_EQ(
            grammar.rulesFor(zbik::NonterminalId{0}),
            (std::vector<zbik::RuleId>{zbik::RuleId{0}, zbik::RuleId{1}}));
}

TEST(GrammarBuilderTest, ExposesNameIndexes) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({"Start -> token"});

    EXPECT_EQ(grammar.findNonterminal("Start"), zbik::NonterminalId{0});
    EXPECT_EQ(grammar.findTerminal("token"), zbik::TerminalId{0});
    EXPECT_EQ(grammar.findTerminal("missing"), std::nullopt);
}

TEST(GrammarBuilderTest, ReportsInvalidArrowWithLineNumber) {
    expectBuildError({"; comment", "S token"}, 2U, "missing '->'");
    expectBuildError({"S -> token -> other"}, 1U, "more than one '->'");
}

TEST(GrammarBuilderTest, RejectsEmptyNonterminalName) {
    expectBuildError({" -> token"}, 1U, "name must not be empty");
}

TEST(GrammarBuilderTest, RejectsGrammarWithoutStartSymbol) {
    expectBuildError({"", "; comment only"}, 1U, "no start symbol");
}

TEST(GrammarBuilderTest, ReportsUnknownSymbolInDeclaredAlphabetMode) {
    try {
        static_cast<void>(zbik::GrammarBuilder{}.build(
                {"; comment", "S -> known typo"},
                {"known"}));
        FAIL() << "Expected GrammarBuildError";
    } catch (const zbik::GrammarBuildError &error) {
        EXPECT_EQ(error.line(), 2U);
        EXPECT_NE(std::string{error.what()}.find("unknown symbol 'typo'"), std::string::npos);
    }
}
