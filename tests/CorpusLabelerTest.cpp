#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "corpus/CorpusLabeler.h"

namespace {

zbik::CorpusLabelOptions quickOptions(std::size_t maxLookahead = 2) {
    return {
            .maxLookahead = maxLookahead,
            .ambiguityMaxLength = 4,
            .ambiguityTreeLimit = 100,
            .ambiguitySeed = 17,
            .collectDetailedReport = true,
            .stopAfterFirstConflict = false,
    };
}

} // namespace

TEST(CorpusLabelerTest, PreservesUncomputedLabelsAndCommentText) {
    const auto result = zbik::labelGrammarCorpus({
            ";[LR(2)] [LALR(2)] [LL(1)] reference text",
            "S -> a",
            "",
            ";[SLR] [notLL(7)] second text",
            "A -> b",
    }, quickOptions());

    ASSERT_EQ(result.unexpectedFailureCount(), 0U);
    ASSERT_EQ(result.grammars.size(), 2U);
    EXPECT_EQ(result.lines[0], ";[LL(1)] [LR(1)] [LALR(1)] reference text");
    EXPECT_EQ(result.lines[3], ";[SLR] [notLL(7)] [LR(1)] [LALR(1)] second text");
}

TEST(CorpusLabelerTest, SkipsDetailedStatisticsUnlessRequested) {
    zbik::CorpusLabelOptions options = quickOptions();
    options.collectDetailedReport = false;
    options.stopAfterFirstConflict = true;
    const auto result = zbik::labelGrammarCorpus({"S -> a"}, options);

    ASSERT_EQ(result.grammars.size(), 1U);
    EXPECT_TRUE(result.grammars.front().lookaheads.empty());
    EXPECT_TRUE(result.grammars.front().automata.empty());
    EXPECT_EQ(result.grammars.front().timings.size(), 4U);
}

TEST(CorpusLabelerTest, FastConflictClassificationMatchesTheCompleteMode) {
    const std::vector<std::string> grammar{
            "S -> a A a",
            "S -> b A b a",
            "A -> b",
            "A ->",
    };
    zbik::CorpusLabelOptions fast = quickOptions(2);
    fast.collectDetailedReport = false;
    fast.stopAfterFirstConflict = true;
    zbik::CorpusLabelOptions complete = fast;
    complete.stopAfterFirstConflict = false;

    const auto fastResult = zbik::labelGrammarCorpus(grammar, fast);
    const auto completeResult = zbik::labelGrammarCorpus(grammar, complete);

    ASSERT_EQ(fastResult.unexpectedFailureCount(), 0U);
    ASSERT_EQ(completeResult.unexpectedFailureCount(), 0U);
    EXPECT_EQ(fastResult.grammars.front().labels,
              completeResult.grammars.front().labels);
    EXPECT_EQ(fastResult.lines, completeResult.lines);
}

TEST(CorpusLabelerTest, LabelsAnAmbiguousGrammarBeforeLrClassification) {
    const auto result = zbik::labelGrammarCorpus({
            "; old classification",
            "S -> A",
            "S -> B",
            "A -> a",
            "B -> a",
    }, quickOptions());

    ASSERT_EQ(result.unexpectedFailureCount(), 0U);
    ASSERT_EQ(result.grammars.size(), 1U);
    EXPECT_EQ(result.lines.front(), ";[ambig] old classification");
    ASSERT_EQ(result.grammars.front().ambiguityStatus,
              zbik::AmbiguitySearchStatus::WitnessFound);
    EXPECT_EQ(result.grammars.front().timings.size(), 3U);
    ASSERT_TRUE(result.grammars.front().ambiguityDiagnostic.has_value());
    EXPECT_NE(result.grammars.front().ambiguityDiagnostic->find("witness-found"),
              std::string::npos);
    ASSERT_EQ(result.grammars.front().lookaheads.size(), 1U);
    EXPECT_TRUE(result.grammars.front().automata.empty());
}

TEST(CorpusLabelerTest, CollectsLookaheadAutomatonTimingAndSourceStatistics) {
    const zbik::CorpusLabelOptions options = quickOptions();
    const auto result = zbik::labelGrammarCorpus({
            "; simple grammar",
            "S -> a",
    }, options);

    ASSERT_EQ(result.unexpectedFailureCount(), 0U);
    ASSERT_EQ(result.grammars.size(), 1U);
    const auto &grammar = result.grammars.front();
    EXPECT_EQ(grammar.source, (std::vector<std::string>{
            "; simple grammar", "S -> a"}));
    ASSERT_EQ(grammar.lookaheads.size(), 1U);
    EXPECT_EQ(grammar.lookaheads[0].k, 1U);
    EXPECT_EQ(grammar.lookaheads[0].firstWords, 1U);
    EXPECT_EQ(grammar.lookaheads[0].followWords, 1U);
    ASSERT_EQ(grammar.lookaheads[0].nonterminals.size(), 1U);
    EXPECT_EQ(grammar.lookaheads[0].nonterminals[0].nonterminal, "S");

    ASSERT_EQ(grammar.automata.size(), 3U);
    EXPECT_EQ(grammar.automata[0].kind, "LR");
    EXPECT_EQ(grammar.automata[0].states, 3U);
    EXPECT_EQ(grammar.automata[0].conflicts, 0U);
    EXPECT_EQ(grammar.automata[1].kind, "LALR");
    EXPECT_EQ(grammar.automata[1].states, 3U);
    EXPECT_EQ(grammar.automata[2].kind, "SLR");
    ASSERT_TRUE(grammar.automata[0].storage);

    const std::string report = zbik::formatCorpusReport(result, options, "Test");
    EXPECT_NE(report.find("GRAMMAR 1"), std::string::npos);
    EXPECT_NE(report.find("S -> a"), std::string::npos);
    EXPECT_NE(report.find("k=1 first_words=1 follow_words=1"), std::string::npos);
    EXPECT_NE(report.find("LR(1) states=3"), std::string::npos);
    EXPECT_NE(report.find("LALR(1) states=3"), std::string::npos);
    EXPECT_NE(report.find("uncompressed_bytes="), std::string::npos);
    EXPECT_NE(report.find("STAGE TOTALS\n"), std::string::npos);
    EXPECT_NE(report.find("ambiguity calls=1 total_seconds="), std::string::npos);
    EXPECT_NE(report.find("LR(1) calls=1 total_seconds="), std::string::npos);
    EXPECT_NE(report.find("max_grammar=1"), std::string::npos);
    EXPECT_NE(report.find("END STAGE TOTALS\n"), std::string::npos);
    EXPECT_NE(report.find("LARGEST SUCCESSFUL TABLE grammar=1"), std::string::npos);
}

TEST(CorpusLabelerTest, ReportsEveryLookaheadAndLrAutomatonThatWasTried) {
    const auto result = zbik::labelGrammarCorpus({
            "S -> a A a",
            "S -> b A b a",
            "A -> b",
            "A ->",
    }, quickOptions(2));

    ASSERT_EQ(result.unexpectedFailureCount(), 0U);
    ASSERT_EQ(result.grammars.size(), 1U);
    const auto &grammar = result.grammars.front();
    ASSERT_EQ(grammar.lookaheads.size(), 2U);
    EXPECT_EQ(grammar.lookaheads[0].k, 1U);
    EXPECT_EQ(grammar.lookaheads[1].k, 2U);
    ASSERT_EQ(grammar.automata.size(), 3U);
    EXPECT_EQ(grammar.automata[0].kind, "LR");
    EXPECT_EQ(grammar.automata[0].k, 1U);
    EXPECT_GT(grammar.automata[0].conflicts, 0U);
    EXPECT_EQ(grammar.automata[1].kind, "LR");
    EXPECT_EQ(grammar.automata[1].k, 2U);
    EXPECT_EQ(grammar.automata[1].conflicts, 0U);
    EXPECT_EQ(grammar.automata[2].kind, "LALR");
    EXPECT_EQ(grammar.automata[2].k, 2U);
}

TEST(CorpusLabelerTest, TreatsGeneratorTrapAsAClassificationNotAFailure) {
    const auto result = zbik::labelGrammarCorpus({
            "S -> S",
            "S -> a",
    }, quickOptions(1));

    ASSERT_EQ(result.unexpectedFailureCount(), 0U);
    ASSERT_EQ(result.grammars.size(), 1U);
    EXPECT_TRUE(result.grammars.front().generatorTrap);
    EXPECT_EQ(result.grammars.front().timings.front().stage, "ambiguity");
    EXPECT_EQ(result.lines.front(), ";[notLR(1)]");
}

TEST(CorpusLabelerTest, PreservesSeparatorsAndReportsUnexpectedBuildErrors) {
    const std::vector<std::string> input{
            "; comments only", "", "---", "", "invalid rule",
    };
    const auto result = zbik::labelGrammarCorpus(input, quickOptions());

    EXPECT_EQ(result.lines, input);
    ASSERT_EQ(result.grammars.size(), 1U);
    EXPECT_EQ(result.unexpectedFailureCount(), 1U);
    ASSERT_TRUE(result.grammars.front().error.has_value());
    EXPECT_NE(result.grammars.front().error->find("missing '->'"), std::string::npos);
}
