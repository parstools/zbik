#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <span>
#include <vector>

#include "first/FirstK.h"
#include "first/Nullable.h"
#include "grammar/GrammarAnalysis.h"
#include "grammar/GrammarBuilder.h"
#include "grammar/GrammarCorpusReader.h"

namespace {

zbik::NonterminalId nt(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findNonterminal(name).value();
}

bool containsEdge(
        std::span<const zbik::GrammarDependencyEdge> edges,
        zbik::RuleId rule, std::size_t position) {
    return std::ranges::any_of(edges, [&](const auto &edge) {
        return edge.rule == rule && edge.rhsPosition == position;
    });
}

std::vector<zbik::GrammarDependencyEdge> expectedEdges(
        const zbik::Grammar &grammar, bool requireNullableSuffix) {
    const zbik::NullableAnalysis nullable(grammar);
    std::vector<zbik::GrammarDependencyEdge> result;
    for (const auto &rule: grammar.rules()) {
        for (std::size_t position = 0; position < rule.size(); ++position) {
            const auto target = std::get_if<zbik::NonterminalId>(&rule.symbol(position));
            if (!target) {
                continue;
            }
            const auto prefix = std::span<const zbik::SymbolRef>{rule.rhs()}.first(position);
            const auto suffix = std::span<const zbik::SymbolRef>{rule.rhs()}.subspan(position + 1);
            if (nullable.isNullable(prefix)
                    && (!requireNullableSuffix || nullable.isNullable(suffix))) {
                result.push_back({rule.lhs(), *target, rule.id(), position});
            }
        }
    }
    std::ranges::sort(result);
    return result;
}

void expectValidWitnesses(
        const zbik::Grammar &grammar,
        std::span<const zbik::GrammarDependencyEdge> edges,
        std::span<const zbik::GrammarCycleComponent> cycles) {
    for (const auto &cycle: cycles) {
        ASSERT_FALSE(cycle.nonterminals.empty());
        ASSERT_FALSE(cycle.witness.empty());
        EXPECT_TRUE(std::ranges::is_sorted(cycle.nonterminals));
        for (std::size_t i = 0; i < cycle.witness.size(); ++i) {
            const auto &edge = cycle.witness[i];
            const auto &next = cycle.witness[(i + 1) % cycle.witness.size()];
            ASSERT_LT(edge.rhsPosition, grammar.rule(edge.rule).size());
            EXPECT_EQ(grammar.rule(edge.rule).lhs(), edge.from);
            EXPECT_EQ(std::get<zbik::NonterminalId>(
                    grammar.rule(edge.rule).symbol(edge.rhsPosition)), edge.to);
            EXPECT_EQ(edge.to, next.from);
            EXPECT_NE(std::ranges::find(edges, edge), edges.end());
            EXPECT_NE(std::ranges::find(cycle.nonterminals, edge.from),
                      cycle.nonterminals.end());
            EXPECT_NE(std::ranges::find(cycle.nonterminals, edge.to),
                      cycle.nonterminals.end());
        }
    }
}

} // namespace

TEST(GrammarAnalysisTest, SeparatesProductivityReachabilityAndUsefulness) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A Dead", "S -> b", "A -> A", "A -> a",
            "Dead -> Dead", "U -> U", "U -> u",
    });
    const zbik::GrammarAnalysis analysis(grammar);

    EXPECT_TRUE(analysis.isProductive(nt(grammar, "S")));
    EXPECT_TRUE(analysis.isProductive(nt(grammar, "A")));
    EXPECT_FALSE(analysis.isProductive(nt(grammar, "Dead")));
    EXPECT_TRUE(analysis.isProductive(nt(grammar, "U")));
    EXPECT_FALSE(analysis.isProductive(zbik::RuleId{0}));
    EXPECT_TRUE(analysis.isProductive(zbik::RuleId{1}));

    EXPECT_TRUE(analysis.isReachable(nt(grammar, "A")));
    EXPECT_TRUE(analysis.isReachable(nt(grammar, "Dead")));
    EXPECT_FALSE(analysis.isReachable(nt(grammar, "U")));
    EXPECT_FALSE(analysis.isReachable(zbik::RuleId{5}));

    EXPECT_TRUE(analysis.isUseful(nt(grammar, "S")));
    EXPECT_FALSE(analysis.isUseful(nt(grammar, "A")));
    EXPECT_FALSE(analysis.isUseful(nt(grammar, "Dead")));
    EXPECT_FALSE(analysis.isUseful(nt(grammar, "U")));
    EXPECT_TRUE(analysis.isUseful(zbik::RuleId{1}));
    EXPECT_FALSE(analysis.isUseful(zbik::RuleId{0}));
    ASSERT_EQ(analysis.zeroProgressCycles().size(), 3U);
    for (const auto &cycle: analysis.zeroProgressCycles()) {
        EXPECT_TRUE(std::ranges::none_of(cycle.nonterminals, [&](const auto id) {
            return analysis.isUseful(id);
        }));
    }
}

TEST(GrammarAnalysisTest, EmptyUsefulnessSetWhenStartIsNotProductive) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A", "A -> A"});
    const zbik::GrammarAnalysis analysis(grammar);
    for (std::size_t i = 0; i < grammar.nonterminalCount(); ++i) {
        EXPECT_FALSE(analysis.isUseful(zbik::NonterminalId{static_cast<std::uint32_t>(i)}));
    }
    for (std::size_t i = 0; i < grammar.ruleCount(); ++i) {
        EXPECT_FALSE(analysis.isUseful(zbik::RuleId{static_cast<std::uint32_t>(i)}));
    }
}

TEST(GrammarAnalysisTest, ProductivityAgreesWithFirstZero) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A B", "S -> token", "A ->", "B -> C", "C -> C",
            "U -> value",
    });
    const zbik::GrammarAnalysis analysis(grammar);
    const zbik::FirstKAnalysis firstZero(grammar, 0);
    for (std::size_t i = 0; i < grammar.nonterminalCount(); ++i) {
        const auto id = zbik::NonterminalId{static_cast<std::uint32_t>(i)};
        EXPECT_EQ(analysis.isProductive(id), !firstZero.first(id).empty());
    }
    for (const auto &rule: grammar.rules()) {
        EXPECT_EQ(analysis.isProductive(rule.id()), !firstZero.first(rule.rhs()).empty());
    }
}

TEST(GrammarAnalysisTest, DistinguishesHiddenLeftRecursionFromZeroProgress) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> N S a", "S -> b", "N ->",
    });
    const zbik::GrammarAnalysis analysis(grammar);

    ASSERT_EQ(analysis.leftRecursiveCycles().size(), 1U);
    EXPECT_EQ(analysis.leftRecursiveCycles()[0].nonterminals,
              (std::vector{nt(grammar, "S")}));
    EXPECT_TRUE(analysis.zeroProgressCycles().empty());
    EXPECT_TRUE(containsEdge(analysis.leftCornerEdges(), zbik::RuleId{0}, 1));
    EXPECT_FALSE(containsEdge(analysis.zeroProgressEdges(), zbik::RuleId{0}, 1));
    EXPECT_TRUE(std::ranges::equal(
            analysis.leftCornerEdges(), expectedEdges(grammar, false)));
    EXPECT_TRUE(std::ranges::equal(
            analysis.zeroProgressEdges(), expectedEdges(grammar, true)));
}

TEST(GrammarAnalysisTest, ClassifiesDirectRecursiveExamples) {
    const auto trappedGrammar = zbik::GrammarBuilder{}.build({"S -> S", "S -> a"});
    const zbik::GrammarAnalysis trapped(trappedGrammar);
    EXPECT_TRUE(trapped.isProductive(trappedGrammar.start()));
    EXPECT_TRUE(trapped.isUseful(trappedGrammar.start()));
    EXPECT_EQ(trapped.zeroProgressCycles().size(), 1U);

    const auto progressingGrammar = zbik::GrammarBuilder{}.build({"S -> S a", "S -> a"});
    const zbik::GrammarAnalysis progressing(progressingGrammar);
    EXPECT_TRUE(progressing.isProductive(progressingGrammar.start()));
    EXPECT_EQ(progressing.leftRecursiveCycles().size(), 1U);
    EXPECT_TRUE(progressing.zeroProgressCycles().empty());

    const auto emptyLanguage = zbik::GrammarBuilder{}.build({"S -> S"});
    const zbik::GrammarAnalysis empty(emptyLanguage);
    EXPECT_FALSE(empty.isProductive(emptyLanguage.start()));
    EXPECT_FALSE(empty.isUseful(emptyLanguage.start()));
    EXPECT_EQ(empty.zeroProgressCycles().size(), 1U);
}

TEST(GrammarAnalysisTest, PreservesRepeatedOccurrencesAndCountsThem) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> B B", "B -> A", "B ->",
    });
    const zbik::GrammarAnalysis analysis(grammar);

    EXPECT_TRUE(containsEdge(analysis.zeroProgressEdges(), zbik::RuleId{1}, 0));
    EXPECT_TRUE(containsEdge(analysis.zeroProgressEdges(), zbik::RuleId{1}, 1));
    ASSERT_EQ(analysis.zeroProgressCycles().size(), 1U);
    EXPECT_EQ(analysis.zeroProgressCycles()[0].nonterminals,
              (std::vector{nt(grammar, "A"), nt(grammar, "B")}));
    EXPECT_EQ(analysis.nonNullableCount(zbik::RuleId{1}), 0U);

    const auto nonNullable = zbik::GrammarBuilder{}.build({"S -> A A", "A -> a"});
    EXPECT_EQ(zbik::GrammarAnalysis(nonNullable).nonNullableCount(zbik::RuleId{0}), 2U);
}

TEST(GrammarAnalysisTest, KeepsDuplicateRulesAsSeparateEdges) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "S -> A", "A -> S", "A ->",
    });
    const zbik::GrammarAnalysis analysis(grammar);
    const auto &edges = analysis.zeroProgressEdges();

    EXPECT_TRUE(containsEdge(edges, zbik::RuleId{0}, 0));
    EXPECT_TRUE(containsEdge(edges, zbik::RuleId{1}, 0));
    EXPECT_TRUE(containsEdge(edges, zbik::RuleId{2}, 0));
    ASSERT_EQ(analysis.zeroProgressCycles().size(), 1U);
    expectValidWitnesses(grammar, edges, analysis.zeroProgressCycles());
}

TEST(GrammarAnalysisTest, ReturnsOneWitnessForEveryCyclicComponent) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "S -> B", "A -> A", "A -> a", "B -> B", "B -> b",
            "C -> c",
    });
    const zbik::GrammarAnalysis first(grammar);
    const zbik::GrammarAnalysis second(grammar);

    ASSERT_EQ(first.zeroProgressCycles().size(), 2U);
    EXPECT_EQ(first.zeroProgressCycles()[0].nonterminals,
              (std::vector{nt(grammar, "A")}));
    EXPECT_EQ(first.zeroProgressCycles()[1].nonterminals,
              (std::vector{nt(grammar, "B")}));
    EXPECT_TRUE(std::ranges::equal(
            first.zeroProgressCycles(), second.zeroProgressCycles()));
    EXPECT_TRUE(std::ranges::equal(
            first.leftRecursiveCycles(), second.leftRecursiveCycles()));
    expectValidWitnesses(grammar, first.zeroProgressEdges(), first.zeroProgressCycles());
    expectValidWitnesses(grammar, first.leftCornerEdges(), first.leftRecursiveCycles());
}

TEST(GrammarAnalysisTest, DoesNotTreatAnAcyclicSingletonAsACycle) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A", "A -> a"});
    const zbik::GrammarAnalysis analysis(grammar);
    EXPECT_TRUE(analysis.zeroProgressCycles().empty());
    EXPECT_TRUE(analysis.leftRecursiveCycles().empty());
}

TEST(GrammarAnalysisTest, DetectsEveryImportedTrapGrammar) {
    const auto path = std::filesystem::path{ZBIK_TEST_RESOURCE_DIR} / "trapGrammars.dat";
    const auto sources = zbik::GrammarCorpusReader::read(path);
    ASSERT_EQ(sources.size(), 5U);
    for (std::size_t i = 0; i < sources.size(); ++i) {
        SCOPED_TRACE(i);
        const auto grammar = zbik::GrammarBuilder{}.build(sources[i]);
        const zbik::GrammarAnalysis analysis(grammar);
        EXPECT_FALSE(analysis.zeroProgressCycles().empty());
        expectValidWitnesses(grammar, analysis.zeroProgressEdges(),
                             analysis.zeroProgressCycles());
    }
}
