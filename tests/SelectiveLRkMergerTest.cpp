#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>

#include "grammar/GrammarBuilder.h"
#include "grammar/GrammarCorpusReader.h"
#include "generator/LRLanguageOracle.h"
#include "lr/CompressedParseTable.h"
#include "lr/LALRkDfa.h"
#include "lr/SelectiveLRkMerger.h"
#include "SelectiveMergeTestSupport.h"

namespace {
using namespace zbik;

void compareWords(const ParseTable &a, const ParseTable &b, std::size_t depth) {
    std::vector<TerminalId> word;
    std::function<void(std::size_t)> visit = [&](std::size_t left) {
        const auto x = selective_test::run(a, word);
        const auto y = selective_test::run(b, word);
        EXPECT_EQ(x.accepted, y.accepted);
        if (x.accepted) {
            EXPECT_EQ(x.reductions, y.reductions);
        }
        if (!left) return;
        for (std::size_t i = 0; i < a.grammar().terminalCount(); ++i) {
            word.push_back(TerminalId{static_cast<std::uint32_t>(i)});
            visit(left - 1);
            word.pop_back();
        }
    };
    visit(depth);
}

TEST(SelectiveLRkMergerTest, MergesRecursiveGrammarForMultipleLookaheads) {
    const auto grammar = GrammarBuilder{}.build({"S -> C C", "C -> c C", "C -> d"});
    for (std::size_t k : {1U, 2U, 3U}) {
        const LRkDfa canonical(grammar, k);
        const auto merged = SelectiveLRkMerger::merge(canonical);
        EXPECT_TRUE(merged.statistics.usedLalr);
        EXPECT_EQ(merged.statistics.attempts, 0U);
        EXPECT_LT(merged.graph.states().size(), canonical.states().size());
        EXPECT_TRUE(SelectiveLRkMerger::validate(canonical, merged,
                    SelectiveMergeMode::CompatibleUnion).empty());
        compareWords(ParseTable(canonical), ParseTable(merged.graph), 7);
        const auto repeat = SelectiveLRkMerger::merge(canonical);
        EXPECT_EQ(merged.canonicalToMerged, repeat.canonicalToMerged);
    }
}

TEST(SelectiveLRkMergerTest, RollsBackMergesWhoseSuccessorsConflict) {
    const auto grammar = GrammarBuilder{}.build({
        "S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e",
        "A -> c c", "B -> c c"});
    const LRkDfa canonical(grammar, 1);
    ASSERT_FALSE(ParseTable(canonical).hasConflicts());
    ASSERT_TRUE(ParseTable(LALRkDfa(canonical)).hasConflicts());
    const auto merged = SelectiveLRkMerger::merge(canonical);
    EXPECT_GT(merged.statistics.rejected, 0U);
    EXPECT_EQ(merged.graph.states().size(), canonical.states().size());
    EXPECT_TRUE(merged.statistics.retainedCanonical);
    EXPECT_EQ(merged.graph.dump(), canonical.dump());
    compareWords(ParseTable(canonical), ParseTable(merged.graph), 5);
}

TEST(SelectiveLRkMergerTest, PreservesActualLr2LanguageAndNullableRules) {
    for (const auto &grammar : {
            GrammarBuilder{}.build({"S -> a A a", "S -> b A b", "A -> a", "A -> a a"}),
            GrammarBuilder{}.build({"S -> a A c", "S -> b A d", "A -> x A", "A ->"})}) {
        const LRkDfa canonical(grammar, 2);
        const auto merged = SelectiveLRkMerger::merge(canonical);
        compareWords(ParseTable(canonical), ParseTable(merged.graph), 5);
    }
}

TEST(SelectiveLRkMergerTest, KeepsUnsafeGroupsApartWhileMergingOtherGroups) {
    const auto grammar = GrammarBuilder{}.build({
        "S -> P", "S -> Q", "P -> a A d", "P -> b B d",
        "P -> a B e", "P -> b A e", "A -> c c", "B -> c c",
        "Q -> C C", "C -> x C", "C -> y"});
    const LRkDfa canonical(grammar, 1);
    const LALRkDfa lalr(canonical);
    const auto merged = SelectiveLRkMerger::merge(canonical);
    EXPECT_LT(merged.graph.states().size(), canonical.states().size());
    EXPECT_GT(merged.graph.states().size(), lalr.states().size());
    EXPECT_FALSE(merged.statistics.retainedCanonical);
    EXPECT_GT(merged.statistics.committed, 0U);
    EXPECT_GT(merged.statistics.rejected, 0U);
    compareWords(ParseTable(canonical), ParseTable(merged.graph), 4);
}

TEST(SelectiveLRkMergerTest, ExactModePreservesErrorsAndZeroBudgetPreservesGraph) {
    const auto grammar = GrammarBuilder{}.build({"S -> C C", "C -> c C", "C -> d"});
    const LRkDfa canonical(grammar, 2);
    const auto exact = SelectiveLRkMerger::merge(canonical,
            {SelectiveMergeMode::ExactActions, 10000});
    EXPECT_TRUE(SelectiveLRkMerger::validate(canonical, exact,
                SelectiveMergeMode::ExactActions).empty());
    compareWords(ParseTable(canonical), ParseTable(exact.graph), 5);
    const auto noBudget = SelectiveLRkMerger::merge(canonical,
            {SelectiveMergeMode::CompatibleUnion, 0});
    EXPECT_TRUE(noBudget.statistics.usedLalr);
    EXPECT_FALSE(noBudget.statistics.budgetExhausted);
    const auto nonLalrGrammar = GrammarBuilder{}.build({
        "X -> Y", "X -> b Y a", "Y -> c", "Y -> c a"});
    const LRkDfa nonLalr(nonLalrGrammar, 2);
    const auto limited = SelectiveLRkMerger::merge(nonLalr,
            {SelectiveMergeMode::CompatibleUnion, 0});
    EXPECT_TRUE(limited.statistics.budgetExhausted);
    EXPECT_TRUE(limited.statistics.retainedCanonical);
    EXPECT_EQ(limited.statistics.attempts, 0U);
    EXPECT_TRUE(std::ranges::equal(nonLalr.states(), limited.graph.states()));
}

TEST(SelectiveLRkMergerTest, RejectsConflictedInputAndCorruptedCertificates) {
    const auto ambiguous = GrammarBuilder{}.build({"S -> S S", "S -> a"});
    EXPECT_THROW((void) SelectiveLRkMerger::merge(LRkDfa(ambiguous, 1)), std::invalid_argument);
    const auto grammar = GrammarBuilder{}.build({"S -> C C", "C -> c C", "C -> d"});
    const LRkDfa canonical(grammar, 2);
    auto result = SelectiveLRkMerger::merge(canonical);
    result.origins.front().push_back(result.origins.front().front());
    EXPECT_FALSE(SelectiveLRkMerger::validate(canonical, result,
                 SelectiveMergeMode::CompatibleUnion).empty());
    result = SelectiveLRkMerger::merge(canonical);
    result.canonicalToMerged.back() = StateId{result.graph.states().size()};
    EXPECT_FALSE(SelectiveLRkMerger::validate(canonical, result,
                 SelectiveMergeMode::CompatibleUnion).empty());
}

TEST(SelectiveLRkMergerTest, ComparesSmallReferenceCorpusExamples) {
    // Snapshot of zubr-kit/src/main/resources/grammars.dat, lines 225 and 240.
    // Keep fixtures local; the test must not depend on a sibling checkout.
    const std::vector<std::vector<std::string>> fixtures{
        {"X -> Y", "X -> b Y a", "Y -> c", "Y -> c a"},
        {"S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e",
         "A -> c", "B -> c"},
    };
    for (std::size_t example = 0; example < fixtures.size(); ++example) {
        const auto grammar = GrammarBuilder{}.build(fixtures[example]);
        for (std::size_t k : {1U, 2U, 3U}) {
            const LRkDfa canonical(grammar, k);
            const LALRkDfa lalr(canonical);
            const ParseTable source(canonical), lalrTable(lalr);
            EXPECT_EQ(canonical.states().size(), example == 0 ? 10U : 14U);
            EXPECT_EQ(lalr.states().size(), example == 0 ? 8U : 13U);
            EXPECT_EQ(source.hasConflicts(), example == 0 && k == 1);
            EXPECT_EQ(lalrTable.conflicts().size(), example == 0 ? 1U : 2U);
            std::cout << "example=" << example + 1 << " k=" << k
                      << " canonical=" << canonical.states().size()
                      << " canonical_conflicts=" << source.conflicts().size()
                      << " lalr=" << lalr.states().size()
                      << " lalr_conflicts=" << lalrTable.conflicts().size();
            if (source.hasConflicts()) {
                std::cout << " selective=not-applicable\n";
                EXPECT_THROW((void) SelectiveLRkMerger::merge(canonical), std::invalid_argument);
                continue;
            }
            const auto merged = SelectiveLRkMerger::merge(canonical);
            EXPECT_FALSE(merged.statistics.usedLalr);
            EXPECT_EQ(merged.statistics.lalrConflicts, lalrTable.conflicts().size());
            const ParseTable target(merged.graph);
            EXPECT_EQ(merged.graph.states().size(), example == 0 ? 9U : 14U);
            EXPECT_EQ(merged.statistics.retainedCanonical, example == 1);
            std::cout << " selective=" << merged.graph.states().size()
                      << " selective_conflicts=" << target.conflicts().size()
                      << " packed_before=" << CompressedParseTable(source).statistics().compressedBytes
                      << " packed_after=" << CompressedParseTable(target).statistics().compressedBytes
                      << '\n';
            EXPECT_FALSE(target.hasConflicts());
            if (!lalrTable.hasConflicts()) {
                EXPECT_EQ(merged.graph.states().size(), lalr.states().size());
            }
            compareWords(source, target, 5);
            EXPECT_TRUE(compareGeneratedLanguage(grammar, target, 5).matches());
        }
    }
}

TEST(SelectiveLRkMergerTest, ClassifiesRawIelrPaperExamplesBeforeConflictResolution) {
    struct Expected {
        std::size_t canonicalStates;
        std::size_t canonicalConflicts;
        std::size_t lalrStates;
        std::size_t lalrConflicts;
        std::optional<std::size_t> selectiveStates;
    };
    const std::array<std::array<Expected, 3>, 6> expected{{
        {{{12, 1, 10, 1, std::nullopt}, {12, 0, 10, 0, 10}, {12, 0, 10, 0, 10}}},
        {{{20, 1, 18, 2, std::nullopt}, {20, 1, 18, 2, std::nullopt},
          {20, 1, 18, 2, std::nullopt}}},
        {{{20, 2, 18, 1, std::nullopt}, {20, 2, 18, 1, std::nullopt},
          {20, 2, 18, 1, std::nullopt}}},
        {{{15, 1, 14, 2, std::nullopt}, {15, 1, 14, 2, std::nullopt},
          {15, 1, 14, 2, std::nullopt}}},
        {{{26, 1, 18, 1, std::nullopt}, {28, 0, 18, 0, 18}, {28, 0, 18, 0, 18}}},
        {{{17, 0, 14, 0, 14}, {17, 0, 14, 0, 14}, {17, 0, 14, 0, 14}}},
    }};
    const auto sources = GrammarCorpusReader::read(
            std::filesystem::path{ZBIK_TEST_RESOURCE_DIR} / "grammars.dat");
    for (std::size_t figure = 1; figure <= 6; ++figure) {
        const std::string marker = ";Denny-Malloy IELR 2010 Fig. " +
                                   std::to_string(figure) + ",";
        const auto source = std::ranges::find_if(sources, [&](const auto &candidate) {
            return !candidate.empty() && candidate.front().starts_with(marker);
        });
        ASSERT_NE(source, sources.end());
        const auto grammar = GrammarBuilder{}.build(*source);
        for (std::size_t k = 1; k <= 3; ++k) {
            SCOPED_TRACE("figure=" + std::to_string(figure) + " k=" + std::to_string(k));
            const auto &want = expected[figure - 1][k - 1];
            const LRkDfa canonical(grammar, k);
            const LALRkDfa lalr(canonical);
            const ParseTable sourceTable(canonical), lalrTable(lalr);
            EXPECT_EQ(canonical.states().size(), want.canonicalStates);
            EXPECT_EQ(sourceTable.conflicts().size(), want.canonicalConflicts);
            EXPECT_EQ(lalr.states().size(), want.lalrStates);
            EXPECT_EQ(lalrTable.conflicts().size(), want.lalrConflicts);
            if (sourceTable.hasConflicts()) {
                EXPECT_FALSE(want.selectiveStates.has_value());
                EXPECT_THROW((void) SelectiveLRkMerger::merge(canonical), std::invalid_argument);
                continue;
            }
            const auto merged = SelectiveLRkMerger::merge(canonical);
            const ParseTable mergedTable(merged.graph);
            ASSERT_TRUE(want.selectiveStates.has_value());
            EXPECT_EQ(merged.graph.states().size(), *want.selectiveStates);
            EXPECT_TRUE(merged.statistics.usedLalr);
            EXPECT_FALSE(mergedTable.hasConflicts());
        }
    }
}
}
