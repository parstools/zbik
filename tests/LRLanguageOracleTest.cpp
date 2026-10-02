#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "generator/LRLanguageOracle.h"
#include "grammar/GrammarBuilder.h"
#include "lr/LALRkDfa.h"
#include "lr/LRkDfa.h"
#include "lr/ParseTable.h"

namespace {

struct LRkOracleCase {
    const char *name;
    std::vector<std::string> rules;
    std::size_t requiredK;
    std::size_t maxWordLength;
    bool verifyLalr;
};

const std::vector<LRkOracleCase> lrkCases{
        {
                "context_dependent_A",
                {"S -> a A a", "S -> b A b a", "A -> b", "A ->"},
                2,
                5,
                true,
        },
        {
                "separate_A_and_A2",
                {"S -> a A a", "S -> b A2 b a", "A -> b", "A ->",
                 "A2 -> b", "A2 ->"},
                2,
                5,
                true,
        },
        {
                "recursive_LL2",
                {"S -> a S A", "S ->", "A -> a b S", "A -> c"},
                2,
                6,
                true,
        },
        {
                "recursive_LL3",
                {"S -> a S A", "S ->", "A -> a a b S", "A -> c"},
                3,
                7,
                true,
        },
        {
                "book_page_148",
                {"X -> Y", "X -> b Y a", "Y -> c", "Y -> c a"},
                2,
                5,
                false,
        },
        {
                "lalr2_shift_reduce",
                {"S -> A a", "A -> c", "A -> c a"},
                2,
                4,
                true,
        },
};

} // namespace

TEST(LRLanguageOracleTest, ComparesEveryShortWordInBothDirections) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a b"});
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));
    const auto result = zbik::compareGeneratedLanguage(grammar, table, 2);

    EXPECT_TRUE(result.matches());
    EXPECT_EQ(result.generatedTrees, 1U);
    EXPECT_EQ(result.generatedWords, 1U);
    EXPECT_EQ(result.testedWords, 7U);
    EXPECT_TRUE(result.mismatches.empty());
}

TEST(LRLanguageOracleTest, IncludesTheEmptyWordAtZeroLength) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S ->"});
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));
    const auto result = zbik::compareGeneratedLanguage(grammar, table, 0);

    EXPECT_TRUE(result.matches());
    EXPECT_EQ(result.generatedTrees, 1U);
    EXPECT_EQ(result.generatedWords, 1U);
    EXPECT_EQ(result.testedWords, 1U);
}

TEST(LRLanguageOracleTest, RejectsConflictingOrDifferentTables) {
    const auto ambiguous = zbik::GrammarBuilder{}.build({
            "S -> E", "E -> E plus E", "E -> id",
    });
    const zbik::ParseTable conflicting(zbik::LRkDfa(ambiguous, 1));
    ASSERT_TRUE(conflicting.hasConflicts());
    EXPECT_THROW(
            static_cast<void>(
                    zbik::compareGeneratedLanguage(ambiguous, conflicting, 3)),
            std::invalid_argument);

    const auto first = zbik::GrammarBuilder{}.build({"S -> a"});
    const auto second = zbik::GrammarBuilder{}.build({"S -> b"});
    const zbik::ParseTable different(zbik::LRkDfa(second, 1));
    EXPECT_THROW(
            static_cast<void>(zbik::compareGeneratedLanguage(first, different, 1)),
            std::invalid_argument);
}

TEST(LRLanguageOracleTest, VerifiesReferenceLRkGrammarsAndTheirMinimumK) {
    for (const auto &test: lrkCases) {
        SCOPED_TRACE(test.name);
        const auto grammar = zbik::GrammarBuilder{}.build(test.rules);

        for (std::size_t k = 1; k < test.requiredK; ++k) {
            const zbik::ParseTable lower(zbik::LRkDfa(grammar, k));
            EXPECT_TRUE(lower.hasConflicts()) << "unexpectedly LR(" << k << ')';
        }

        const zbik::LRkDfa canonical(grammar, test.requiredK);
        const zbik::ParseTable canonicalTable(canonical);
        ASSERT_FALSE(canonicalTable.hasConflicts());
        const auto canonicalResult = zbik::compareGeneratedLanguage(
                grammar, canonicalTable, test.maxWordLength);
        EXPECT_TRUE(canonicalResult.matches());
        EXPECT_GT(canonicalResult.generatedWords, 0U);

        const zbik::ParseTable lalrTable{zbik::LALRkDfa(canonical)};
        if (test.verifyLalr) {
            ASSERT_FALSE(lalrTable.hasConflicts());
            const auto lalrResult = zbik::compareGeneratedLanguage(
                    grammar, lalrTable, test.maxWordLength);
            EXPECT_TRUE(lalrResult.matches());
            EXPECT_EQ(lalrResult.generatedWords, canonicalResult.generatedWords);
            EXPECT_EQ(lalrResult.testedWords, canonicalResult.testedWords);
        } else {
            EXPECT_TRUE(lalrTable.hasConflicts());
        }
    }
}
