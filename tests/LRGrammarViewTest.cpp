#include <gtest/gtest.h>

#include "grammar/GrammarBuilder.h"
#include "lr/LRGrammarView.h"

TEST(LRGrammarViewTest, AddsSyntheticStartWithoutMutatingGrammar) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> token",
    });
    const std::string originalDump = grammar.dump();

    const zbik::LRGrammarView firstView(grammar);
    const zbik::LRGrammarView secondView(grammar);

    EXPECT_EQ(firstView.start(), zbik::NonterminalId{1});
    EXPECT_EQ(firstView.syntheticRuleId(), zbik::RuleId{1});
    EXPECT_EQ(firstView.originalStart(), zbik::NonterminalId{0});
    EXPECT_EQ(firstView.nonterminalCount(), 2U);
    EXPECT_EQ(firstView.ruleCount(), 2U);
    EXPECT_EQ(firstView.terminalCount(), 1U);
    EXPECT_EQ(firstView.nonterminalName(firstView.start()), "S′");

    const zbik::Rule &synthetic = firstView.syntheticRule();
    EXPECT_EQ(synthetic.id(), firstView.syntheticRuleId());
    EXPECT_EQ(synthetic.lhs(), firstView.start());
    EXPECT_EQ(synthetic.rhs(), (std::vector<zbik::SymbolRef>{grammar.start()}));
    EXPECT_EQ(firstView.rulesFor(firstView.start()), (std::vector{firstView.syntheticRuleId()}));

    EXPECT_EQ(secondView.start(), firstView.start());
    EXPECT_EQ(secondView.syntheticRuleId(), firstView.syntheticRuleId());
    EXPECT_EQ(grammar.ruleCount(), 1U);
    EXPECT_EQ(grammar.nonterminalCount(), 1U);
    EXPECT_EQ(grammar.dump(), originalDump);
}

TEST(LRGrammarViewTest, KeepsUserRuleIdsAndProvidesUniformLookup) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "A -> token",
    });
    const zbik::LRGrammarView view(grammar);

    EXPECT_EQ(view.rule(zbik::RuleId{0}).id(), zbik::RuleId{0});
    EXPECT_EQ(view.rule(zbik::RuleId{1}).id(), zbik::RuleId{1});
    EXPECT_EQ(view.rule(zbik::RuleId{2}).id(), zbik::RuleId{2});
    EXPECT_TRUE(view.isSynthetic(zbik::RuleId{2}));
    EXPECT_TRUE(view.isSynthetic(zbik::NonterminalId{2}));
    EXPECT_EQ(view.symbolName(zbik::SymbolRef{zbik::NonterminalId{2}}), "S′");
}

TEST(LRGrammarViewTest, ChoosesUniqueSyntheticName) {
    const zbik::Grammar grammar = zbik::GrammarBuilder{}.build({
            "S -> S′",
            "S′ -> token",
    });
    const zbik::LRGrammarView view(grammar);

    EXPECT_EQ(view.nonterminalName(view.start()), "S′′");
}
