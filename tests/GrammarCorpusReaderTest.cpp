#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "grammar/GrammarBuilder.h"
#include "grammar/GrammarCorpusReader.h"
#include "generator/AmbiguitySearch.h"
#include "generator/LRLanguageOracle.h"
#include "lr/DirectLALRkDfa.h"
#include "lr/LALRkDfa.h"

TEST(GrammarCorpusReaderTest, SplitsCorpusWithoutBuildingGrammars) {
    const auto sources = zbik::GrammarCorpusReader::split({
            "; first grammar",
            "S -> a",
            "",
            "; second grammar",
            "A ->",
            "---",
            "; comment-only block",
            "",
            "=== metadata separator",
            "B -> b",
    });

    ASSERT_EQ(sources.size(), 3U);
    EXPECT_EQ(sources[0], (zbik::GrammarSource{"; first grammar", "S -> a"}));
    EXPECT_EQ(sources[1], (zbik::GrammarSource{"; second grammar", "A ->"}));
    EXPECT_EQ(sources[2], (zbik::GrammarSource{"B -> b"}));
}

TEST(GrammarCorpusReaderTest, ReadsAndBuildsProjectCorpus) {
    const auto path = std::filesystem::path{ZBIK_TEST_RESOURCE_DIR} / "grammars.dat";
    const auto sources = zbik::GrammarCorpusReader::read(path);

    ASSERT_GT(sources.size(), 40U);
    for (std::size_t i = 0; i < sources.size(); ++i) {
        SCOPED_TRACE(i);
        const zbik::Grammar grammar = zbik::GrammarBuilder{}.build(sources[i]);
        EXPECT_NE(grammar.ruleCount(), 0U);
    }
}

TEST(GrammarCorpusReaderTest, IncludesIelrPaperExamplesWithRawGrammarSemantics) {
    const auto sources = zbik::GrammarCorpusReader::read(
            std::filesystem::path{ZBIK_TEST_RESOURCE_DIR} / "grammars.dat");
    for (std::size_t figure = 1; figure <= 6; ++figure) {
        SCOPED_TRACE(figure);
        const std::string marker = ";Denny-Malloy IELR 2010 Fig. " +
                                   std::to_string(figure) + ",";
        std::size_t matches = 0;
        for (const auto &source : sources) {
            if (source.empty() || !source.front().starts_with(marker)) continue;
            ++matches;
            const auto grammar = zbik::GrammarBuilder{}.build(source);
            if (figure >= 2 && figure <= 4) {
                EXPECT_EQ(zbik::findAmbiguity(grammar, {.maxLength = 4}).status(),
                          zbik::AmbiguitySearchStatus::WitnessFound);
                EXPECT_TRUE(zbik::ParseTable(zbik::LRkDfa(grammar, 1)).hasConflicts());
            }
            if (figure == 1) {
                EXPECT_TRUE(zbik::ParseTable(zbik::LRkDfa(grammar, 1)).hasConflicts());
                const zbik::ParseTable table(zbik::LRkDfa(grammar, 2));
                ASSERT_FALSE(table.hasConflicts());
                const auto oracle = zbik::compareGeneratedLanguage(grammar, table, 5);
                EXPECT_EQ(oracle.generatedWords, 4U);
                EXPECT_TRUE(oracle.matches());
            }
        }
        EXPECT_EQ(matches, 1U);
    }
}

TEST(GrammarCorpusReaderTest, DirectLalrMatchesCanonicalMergingForTheCorpus) {
    const auto sources = zbik::GrammarCorpusReader::read(
            std::filesystem::path{ZBIK_TEST_RESOURCE_DIR} / "grammars.dat");
    for (std::size_t source = 0; source < sources.size(); ++source) {
        const auto grammar = zbik::GrammarBuilder{}.build(sources[source]);
        for (std::size_t k : {1U, 2U}) {
            SCOPED_TRACE("grammar=" + std::to_string(source + 1) +
                         " k=" + std::to_string(k));
            const zbik::LALRkDfa reference(zbik::LRkDfa(grammar, k));
            const zbik::DirectLALRkDfa direct(grammar, k);
            EXPECT_TRUE(std::ranges::equal(reference.states(), direct.states()));
            EXPECT_EQ(zbik::ParseTable(reference).dump(),
                      zbik::ParseTable(direct).dump());
        }
    }
}
