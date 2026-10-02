#include <gtest/gtest.h>

#include <string>

#include "generator/AmbiguitySearch.h"
#include "grammar/GrammarBuilder.h"

TEST(AmbiguitySearchTest, ReturnsTwoCompleteTreesForDuplicateProductions) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> token", "S -> token"});
    const auto result = zbik::findAmbiguity(grammar, {.maxLength = 1});

    EXPECT_EQ(result.status(), zbik::AmbiguitySearchStatus::WitnessFound);
    EXPECT_EQ(result.examinedTrees(), 2U);
    EXPECT_EQ(result.maxLength(), 1U);
    EXPECT_EQ(result.order(), zbik::DerivationOrder::RuleId);
    ASSERT_TRUE(result.witness());
    EXPECT_EQ(result.witness()->word, (std::vector{zbik::TerminalId{0}}));
    EXPECT_EQ(result.witness()->first.terminals(), result.witness()->word);
    EXPECT_EQ(result.witness()->second.terminals(), result.witness()->word);
    EXPECT_NE(result.witness()->first, result.witness()->second);
    EXPECT_EQ(result.witness()->first.rule(), zbik::RuleId{0});
    EXPECT_EQ(result.witness()->second.rule(), zbik::RuleId{1});

    const std::string dump = result.dump(grammar);
    EXPECT_NE(dump.find("witness-found: maxLen=1 examined=2"), std::string::npos);
    EXPECT_NE(dump.find("word=[\"token\"]"), std::string::npos);
    EXPECT_NE(dump.find("first=S#R0"), std::string::npos);
    EXPECT_NE(dump.find("second=S#R1"), std::string::npos);
}

TEST(AmbiguitySearchTest, FindsStructuralAmbiguityInRecursiveGrammar) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> S S", "S -> a"});
    const auto result = zbik::findAmbiguity(grammar, {.maxLength = 3});

    ASSERT_EQ(result.status(), zbik::AmbiguitySearchStatus::WitnessFound);
    ASSERT_TRUE(result.witness());
    EXPECT_EQ(result.witness()->word,
              (std::vector{zbik::TerminalId{0}, zbik::TerminalId{0},
                           zbik::TerminalId{0}}));
    EXPECT_NE(result.witness()->first.structuralFingerprint(),
              result.witness()->second.structuralFingerprint());
}

TEST(AmbiguitySearchTest, FindsTwoDerivationsOfEpsilon) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "S -> B", "A ->", "B ->",
    });
    const auto result = zbik::findAmbiguity(grammar, {.maxLength = 0});

    ASSERT_EQ(result.status(), zbik::AmbiguitySearchStatus::WitnessFound);
    ASSERT_TRUE(result.witness());
    EXPECT_TRUE(result.witness()->word.empty());
    EXPECT_TRUE(result.witness()->first.terminals().empty());
    EXPECT_TRUE(result.witness()->second.terminals().empty());
}

TEST(AmbiguitySearchTest, KeepsTheClaimBoundedByMaxLength) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a b", "S -> A b", "A -> a",
    });
    const auto shorter = zbik::findAmbiguity(grammar, {.maxLength = 1});
    const auto sufficient = zbik::findAmbiguity(grammar, {.maxLength = 2});

    EXPECT_EQ(shorter.status(), zbik::AmbiguitySearchStatus::Exhausted);
    EXPECT_EQ(shorter.examinedTrees(), 0U);
    EXPECT_EQ(sufficient.status(), zbik::AmbiguitySearchStatus::WitnessFound);
}

TEST(AmbiguitySearchTest, ReportsExhaustionOnlyAfterCompleteBoundedSearch) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a S", "S ->"});
    const auto result = zbik::findAmbiguity(grammar, {.maxLength = 3});

    EXPECT_EQ(result.status(), zbik::AmbiguitySearchStatus::Exhausted);
    EXPECT_EQ(result.examinedTrees(), 4U);
    EXPECT_FALSE(result.treeLimit());
    EXPECT_FALSE(result.witness());
    EXPECT_NE(result.dump(grammar).find(
                      "no ambiguity witness exists within maxLen"),
              std::string::npos);
}

TEST(AmbiguitySearchTest, UsesStableShuffleByDefaultForLimitedSearch) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A A A", "A -> a", "A -> b", "A -> c",
    });
    const zbik::AmbiguitySearchOptions options{
            .maxLength = 3, .treeLimit = 5, .seed = 12345,
    };
    const auto first = zbik::findAmbiguity(grammar, options);
    const auto second = zbik::findAmbiguity(grammar, options);

    EXPECT_EQ(first.status(), zbik::AmbiguitySearchStatus::Inconclusive);
    EXPECT_EQ(first.examinedTrees(), 5U);
    EXPECT_EQ(first.treeLimit(), 5U);
    EXPECT_EQ(first.order(), zbik::DerivationOrder::StableShuffle);
    EXPECT_EQ(first.seed(), 12345U);
    EXPECT_EQ(first.dump(grammar), second.dump(grammar));
    EXPECT_NE(first.dump(grammar).find("limit=5"), std::string::npos);
    EXPECT_NE(first.dump(grammar).find("seed=12345"), std::string::npos);
    EXPECT_NE(first.dump(grammar).find("tree limit reached before exhaustion"),
              std::string::npos);
}

TEST(AmbiguitySearchTest, DetectsExactExhaustionAtTheTreeLimit) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a", "S -> b"});
    const auto result = zbik::findAmbiguity(
            grammar, {.maxLength = 1, .treeLimit = 2, .seed = 7});

    EXPECT_EQ(result.status(), zbik::AmbiguitySearchStatus::Exhausted);
    EXPECT_EQ(result.examinedTrees(), 2U);
    EXPECT_EQ(result.order(), zbik::DerivationOrder::StableShuffle);
}

TEST(AmbiguitySearchTest, CanFindAWitnessAtTheConfiguredLimit) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a", "S -> a"});
    const auto result = zbik::findAmbiguity(
            grammar, {.maxLength = 1, .treeLimit = 2, .seed = 17});

    EXPECT_EQ(result.status(), zbik::AmbiguitySearchStatus::WitnessFound);
    EXPECT_EQ(result.examinedTrees(), 2U);
    ASSERT_TRUE(result.witness());
}

TEST(AmbiguitySearchTest, DefinesZeroTreeLimitWithoutInspectingAPeekedTree) {
    const auto productive = zbik::GrammarBuilder{}.build({"S ->"});
    const auto productiveResult = zbik::findAmbiguity(
            productive, {.maxLength = 0, .treeLimit = 0});
    EXPECT_EQ(productiveResult.status(), zbik::AmbiguitySearchStatus::Inconclusive);
    EXPECT_EQ(productiveResult.examinedTrees(), 0U);

    const auto empty = zbik::GrammarBuilder{}.build({"S -> S"});
    const auto emptyResult = zbik::findAmbiguity(
            empty, {.maxLength = 0, .treeLimit = 0});
    EXPECT_EQ(emptyResult.status(), zbik::AmbiguitySearchStatus::Exhausted);
    EXPECT_EQ(emptyResult.examinedTrees(), 0U);
}

TEST(AmbiguitySearchTest, AllowsExplicitRuleIdOrderForLimitedSearch) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a", "S -> b", "S -> c"});
    const auto result = zbik::findAmbiguity(
            grammar,
            {.maxLength = 1,
             .treeLimit = 1,
             .order = zbik::DerivationOrder::RuleId,
             .seed = 99});

    EXPECT_EQ(result.status(), zbik::AmbiguitySearchStatus::Inconclusive);
    EXPECT_EQ(result.order(), zbik::DerivationOrder::RuleId);
    EXPECT_EQ(result.seed(), 99U);
    EXPECT_EQ(result.dump(grammar).find("seed="), std::string::npos);
}

TEST(AmbiguitySearchTest, RejectsAUsefulZeroProgressCycle) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> S", "S -> a"});

    EXPECT_THROW(
            static_cast<void>(zbik::findAmbiguity(grammar, {.maxLength = 1})),
            zbik::GeneratorGrammarError);
}
