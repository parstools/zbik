#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "ebnf/EbnfGrammar.h"

TEST(EbnfGrammarTest, PreservesRuleAlternativeAndElementOrder) {
    const zbik::EbnfGrammarSpec grammar{
            zbik::EbnfRuleSpec{"Start",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"prefix"},
                                               zbik::EbnfElement{"item", zbik::Repetition::OneOrMore},
                                       },
                                       zbik::EbnfAlternative{},
                               }},
            zbik::EbnfRuleSpec{"item",
                               {
                                       zbik::EbnfAlternative{
                                               zbik::EbnfElement{"name", zbik::Repetition::Optional},
                                               zbik::EbnfElement{"tail", zbik::Repetition::ZeroOrMore},
                                       },
                               }},
    };

    ASSERT_EQ(grammar.rules().size(), 2U);
    EXPECT_EQ(grammar.startSymbol(), "Start");
    EXPECT_EQ(grammar.rules()[0].lhs(), "Start");
    ASSERT_EQ(grammar.rules()[0].alternatives().size(), 2U);
    ASSERT_EQ(grammar.rules()[0].alternatives()[0].elements().size(), 2U);
    EXPECT_EQ(grammar.rules()[0].alternatives()[0].elements()[0].symbol(), "prefix");
    EXPECT_EQ(grammar.rules()[0].alternatives()[0].elements()[0].repetition(), zbik::Repetition::One);
    EXPECT_EQ(grammar.rules()[0].alternatives()[0].elements()[1].repetition(), zbik::Repetition::OneOrMore);
    EXPECT_TRUE(grammar.rules()[0].alternatives()[1].empty());
    EXPECT_EQ(grammar.rules()[1].alternatives()[0].elements()[0].repetition(), zbik::Repetition::Optional);
    EXPECT_EQ(grammar.rules()[1].alternatives()[0].elements()[1].repetition(), zbik::Repetition::ZeroOrMore);
}

TEST(EbnfGrammarTest, OwnsInputValuesAndExposesOnlyConstViews) {
    std::string symbol = "token";
    std::vector<zbik::EbnfElement> elements;
    elements.emplace_back(symbol);
    std::vector<zbik::EbnfAlternative> alternatives;
    alternatives.emplace_back(elements);
    std::vector<zbik::EbnfRuleSpec> rules;
    rules.emplace_back("Start", alternatives);

    const zbik::EbnfGrammarSpec grammar{rules};
    symbol = "changed";
    elements.clear();
    alternatives.clear();
    rules.clear();

    EXPECT_EQ(grammar.rules()[0].alternatives()[0].elements()[0].symbol(), "token");
}

TEST(EbnfGrammarTest, RejectsInvalidNamesAndRulesWithoutAlternatives) {
    EXPECT_THROW(zbik::EbnfElement{""}, std::invalid_argument);
    EXPECT_THROW(zbik::EbnfElement{"two symbols"}, std::invalid_argument);
    EXPECT_THROW((zbik::EbnfRuleSpec{"", {zbik::EbnfAlternative{}}}), std::invalid_argument);
    EXPECT_THROW((zbik::EbnfRuleSpec{"Start", std::vector<zbik::EbnfAlternative>{}}), std::invalid_argument);
}

TEST(EbnfGrammarTest, RejectsAnEmptyGrammarAndDuplicateRuleNames) {
    EXPECT_THROW(zbik::EbnfGrammarSpec(std::vector<zbik::EbnfRuleSpec>{}), std::invalid_argument);
    EXPECT_THROW((zbik::EbnfGrammarSpec{
                         zbik::EbnfRuleSpec{"Start", {zbik::EbnfAlternative{}}},
                         zbik::EbnfRuleSpec{"Start", {zbik::EbnfAlternative{zbik::EbnfElement{"token"}}}},
                 }),
                 std::invalid_argument);
}
