#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "generator/BoundedDerivationGenerator.h"
#include "grammar/GrammarBuilder.h"

namespace {

struct Generated {
    std::vector<zbik::TerminalId> terminals;
    std::string fingerprint;
};

std::vector<Generated> collect(
        zbik::BoundedDerivationGenerator &generator,
        std::size_t safetyLimit = 1000) {
    std::vector<Generated> result;
    while (generator.next()) {
        if (result.size() == safetyLimit) {
            throw std::runtime_error("generator exceeded the test safety limit");
        }
        const auto &tree = generator.currentTree();
        result.push_back({tree.terminals(), tree.structuralFingerprint()});
    }
    return result;
}

std::vector<std::size_t> lengths(const std::vector<Generated> &generated) {
    std::vector<std::size_t> result;
    for (const auto &entry: generated) result.push_back(entry.terminals.size());
    std::ranges::sort(result);
    return result;
}

std::vector<std::string> fingerprints(const std::vector<Generated> &generated) {
    std::vector<std::string> result;
    for (const auto &entry: generated) result.push_back(entry.fingerprint);
    return result;
}

} // namespace

TEST(BoundedDerivationGeneratorTest, EnumeratesTheCompleteCartesianProduct) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A A", "A -> a", "A ->",
    });
    zbik::BoundedDerivationGenerator generator(grammar, 2);
    const auto generated = collect(generator);

    ASSERT_EQ(generated.size(), 4U);
    EXPECT_EQ(lengths(generated), (std::vector<std::size_t>{0, 1, 1, 2}));
    std::vector<std::string> fingerprints;
    for (const auto &entry: generated) fingerprints.push_back(entry.fingerprint);
    std::ranges::sort(fingerprints);
    EXPECT_EQ(std::ranges::unique(fingerprints).begin(), fingerprints.end());
}

TEST(BoundedDerivationGeneratorTest, TreatsMaxLengthAsAnUpperBound) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S ->", "S -> a", "S -> a b",
    });
    zbik::BoundedDerivationGenerator generator(grammar, 2);

    EXPECT_EQ(lengths(collect(generator)),
              (std::vector<std::size_t>{0, 1, 2}));
}

TEST(BoundedDerivationGeneratorTest, ReservesMinimumLengthForTheRemainingSuffix) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A B", "A -> a A", "A ->", "B -> b B", "B ->",
    });
    zbik::BoundedDerivationGenerator generator(grammar, 2);
    const auto generated = collect(generator);

    ASSERT_EQ(generated.size(), 6U);
    EXPECT_EQ(lengths(generated),
              (std::vector<std::size_t>{0, 1, 1, 2, 2, 2}));
}

TEST(BoundedDerivationGeneratorTest, TerminatesOnProgressingLeftRecursion) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> S a", "S -> b"});
    zbik::BoundedDerivationGenerator generator(grammar, 3);
    const auto generated = collect(generator);

    EXPECT_EQ(lengths(generated), (std::vector<std::size_t>{1, 2, 3}));
}

TEST(BoundedDerivationGeneratorTest, HandlesHiddenLeftRecursionWithProgress) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> N S a", "S -> b", "N ->",
    });
    zbik::BoundedDerivationGenerator generator(grammar, 3);

    EXPECT_EQ(lengths(collect(generator)),
              (std::vector<std::size_t>{1, 2, 3}));
}

TEST(BoundedDerivationGeneratorTest, PreservesAmbiguousBranchingTrees) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> S S", "S -> a"});
    zbik::BoundedDerivationGenerator generator(grammar, 3);
    const auto generated = collect(generator);

    ASSERT_EQ(generated.size(), 4U);
    EXPECT_EQ(lengths(generated), (std::vector<std::size_t>{1, 2, 3, 3}));
    std::vector<std::string> lengthThree;
    for (const auto &entry: generated) {
        if (entry.terminals.size() == 3) lengthThree.push_back(entry.fingerprint);
    }
    ASSERT_EQ(lengthThree.size(), 2U);
    EXPECT_NE(lengthThree[0], lengthThree[1]);
}

TEST(BoundedDerivationGeneratorTest, PreservesDuplicateProductions) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a", "S -> a"});
    zbik::BoundedDerivationGenerator generator(grammar, 1);
    const auto generated = collect(generator);

    ASSERT_EQ(generated.size(), 2U);
    EXPECT_EQ(generated[0].terminals, generated[1].terminals);
    EXPECT_NE(generated[0].fingerprint, generated[1].fingerprint);
}

TEST(BoundedDerivationGeneratorTest, RejectsOnlyAUsefulZeroProgressCycle) {
    const auto trapped = zbik::GrammarBuilder{}.build({"S -> S", "S -> a"});
    try {
        const zbik::BoundedDerivationGenerator generator(trapped, 1);
        FAIL() << "expected a useful zero-progress cycle to be rejected";
    } catch (const zbik::GeneratorGrammarError &error) {
        ASSERT_EQ(error.cycle().nonterminals.size(), 1U);
        EXPECT_EQ(error.cycle().nonterminals.front(), trapped.start());
        ASSERT_EQ(error.cycle().witness.size(), 1U);
        EXPECT_EQ(error.cycle().witness.front().rule, zbik::RuleId{0});
    }

    const auto empty = zbik::GrammarBuilder{}.build({"S -> S"});
    zbik::BoundedDerivationGenerator emptyGenerator(empty, 3);
    EXPECT_FALSE(emptyGenerator.next());

    const auto unreachable = zbik::GrammarBuilder{}.build({
            "S -> a", "U -> U", "U -> u",
    });
    zbik::BoundedDerivationGenerator unreachableGenerator(unreachable, 1);
    EXPECT_EQ(collect(unreachableGenerator).size(), 1U);

    const auto useless = zbik::GrammarBuilder{}.build({
            "S -> A Dead", "S -> b", "A -> A", "A -> a", "Dead -> Dead",
    });
    zbik::BoundedDerivationGenerator uselessGenerator(useless, 1);
    EXPECT_EQ(collect(uselessGenerator).size(), 1U);
}

TEST(BoundedDerivationGeneratorTest, SupportsZeroBudgetAndEmptyProductions) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S ->", "S -> token"});
    zbik::BoundedDerivationGenerator generator(grammar, 0);
    const auto generated = collect(generator);

    ASSERT_EQ(generated.size(), 1U);
    EXPECT_TRUE(generated.front().terminals.empty());
}

TEST(BoundedDerivationGeneratorTest, DefinesCurrentTreeLifetimeAndExhaustion) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> token"});
    zbik::BoundedDerivationGenerator generator(grammar, 1);

    EXPECT_EQ(generator.maxLength(), 1U);
    EXPECT_EQ(&generator.grammar(), &grammar);
    EXPECT_FALSE(generator.hasCurrent());
    EXPECT_FALSE(generator.exhausted());
    EXPECT_THROW(static_cast<void>(generator.currentTree()), std::logic_error);

    ASSERT_TRUE(generator.next());
    EXPECT_TRUE(generator.hasCurrent());
    EXPECT_FALSE(generator.exhausted());
    EXPECT_EQ(generator.currentTree().terminals(),
              (std::vector{zbik::TerminalId{0}}));

    EXPECT_FALSE(generator.next());
    EXPECT_FALSE(generator.hasCurrent());
    EXPECT_TRUE(generator.exhausted());
    EXPECT_THROW(static_cast<void>(generator.currentTree()), std::logic_error);
    EXPECT_FALSE(generator.next());
}

TEST(BoundedDerivationGeneratorTest, IsDeterministicForTheSameGrammar) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A A", "A -> a A", "A ->",
    });
    zbik::BoundedDerivationGenerator first(grammar, 3);
    zbik::BoundedDerivationGenerator second(grammar, 3);
    const auto firstGenerated = collect(first);
    const auto secondGenerated = collect(second);

    ASSERT_EQ(firstGenerated.size(), secondGenerated.size());
    for (std::size_t i = 0; i < firstGenerated.size(); ++i) {
        EXPECT_EQ(firstGenerated[i].terminals, secondGenerated[i].terminals);
        EXPECT_EQ(firstGenerated[i].fingerprint, secondGenerated[i].fingerprint);
    }
}

TEST(BoundedDerivationGeneratorTest, StableShuffleChangesOrderButNotCompleteness) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A A A", "A -> a", "A -> b", "A -> c",
    });
    zbik::BoundedDerivationGenerator byRuleId(grammar, 3);
    zbik::BoundedDerivationGenerator shuffledA(
            grammar, 3, zbik::DerivationOrder::StableShuffle, 12345);
    zbik::BoundedDerivationGenerator shuffledB(
            grammar, 3, zbik::DerivationOrder::StableShuffle, 12345);
    zbik::BoundedDerivationGenerator shuffledOther(
            grammar, 3, zbik::DerivationOrder::StableShuffle, 54321);

    const auto byRuleIdResults = fingerprints(collect(byRuleId));
    const auto shuffledAResults = fingerprints(collect(shuffledA));
    const auto shuffledBResults = fingerprints(collect(shuffledB));
    const auto shuffledOtherResults = fingerprints(collect(shuffledOther));

    EXPECT_EQ(shuffledA.order(), zbik::DerivationOrder::StableShuffle);
    EXPECT_EQ(shuffledA.seed(), 12345U);
    EXPECT_EQ(shuffledAResults, shuffledBResults);
    EXPECT_NE(shuffledAResults, byRuleIdResults);
    EXPECT_NE(shuffledAResults, shuffledOtherResults);

    auto expectedSet = byRuleIdResults;
    auto shuffledSet = shuffledAResults;
    std::ranges::sort(expectedSet);
    std::ranges::sort(shuffledSet);
    EXPECT_EQ(shuffledSet, expectedSet);
}

TEST(BoundedDerivationGeneratorTest, MatchesTheReferenceLlStarTreeCount) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a S C", "S ->", "C -> A b S", "C -> c",
            "A -> a A", "A ->",
    });
    zbik::BoundedDerivationGenerator generator(grammar, 9);

    EXPECT_EQ(collect(generator, 1000).size(), 617U);
}
