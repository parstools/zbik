#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lr/DirectLALRkDfa.h"
#include "lr/LALRkDfa.h"
#include "lr/LRMachine.h"

namespace {

static_assert(!std::is_convertible_v<const zbik::LALRkDfa &, const zbik::LRkDfa &>);
static_assert(!std::is_assignable_v<zbik::LRkDfa &, const zbik::LALRkDfa &>);

std::set<zbik::Item> itemsFromOrigins(
        const zbik::LALRkDfa &lalr, zbik::StateId state) {
    std::set<zbik::Item> result;
    for (const zbik::StateId origin: lalr.canonicalOrigins(state)) {
        const zbik::ItemSet &items = lalr.canonical().state(origin).items;
        result.insert(items.begin(), items.end());
    }
    return result;
}

zbik::StateId mergedOrigin(const zbik::LALRkDfa &lalr, zbik::StateId canonical) {
    for (std::size_t index = 0; index < lalr.originGroups().size(); ++index) {
        if (std::ranges::find(lalr.originGroups()[index], canonical)
                != lalr.originGroups()[index].end()) {
            return zbik::StateId{index};
        }
    }
    throw std::logic_error("canonical state has no LALR origin group");
}

} // namespace

TEST(LALRkDfaTest, MergesEqualCoresAndPreservesRecognition) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> C C", "C -> c C", "C -> d",
    });
    const zbik::LRkDfa canonical(grammar, 1);
    const zbik::LALRkDfa lalr(canonical);
    const zbik::ParseTable table(lalr);
    const zbik::LRMachine machine(table);
    const auto c = grammar.findTerminal("c").value();
    const auto d = grammar.findTerminal("d").value();

    EXPECT_EQ(canonical.states().size(), 10U);
    EXPECT_EQ(lalr.states().size(), 7U);
    EXPECT_TRUE(table.isLalr());
    EXPECT_FALSE(table.hasConflicts());
    EXPECT_TRUE(table.mergeConflicts().empty());
    EXPECT_EQ(lalr.dump().find("LALR(1) states=7"), 0U);
    EXPECT_EQ(lalr.toDot().find("digraph LALRkDfa"), 0U);
    EXPECT_EQ(table.dump().find("LALR(1) table"), 0U);
    EXPECT_TRUE(machine.parse(std::vector{c, d, d}).accepted);
    EXPECT_TRUE(machine.parse(std::vector{d, d}).accepted);
    EXPECT_FALSE(machine.parse(std::vector{d}).accepted);
}

TEST(LALRkDfaTest, RetainsOriginsItemsAndRemappedTransitions) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> C C", "C -> c C", "C -> d",
    });
    const zbik::LALRkDfa lalr(grammar, 2);
    std::vector<bool> seen(lalr.canonical().states().size(), false);

    ASSERT_EQ(lalr.originGroups().size(), lalr.states().size());
    for (std::size_t index = 0; index < lalr.states().size(); ++index) {
        const zbik::StateId merged{index};
        const auto expectedItems = itemsFromOrigins(lalr, merged);
        EXPECT_TRUE(std::ranges::equal(lalr.state(merged).items, expectedItems));
        for (const zbik::StateId origin: lalr.canonicalOrigins(merged)) {
            ASSERT_LT(origin.value, seen.size());
            EXPECT_FALSE(seen[origin.value]);
            seen[origin.value] = true;
            for (const auto &[symbol, canonicalTarget]:
                    lalr.canonical().state(origin).transitions) {
                const auto transition = lalr.state(merged).transitions.find(symbol);
                ASSERT_NE(transition, lalr.state(merged).transitions.end());
                EXPECT_EQ(transition->second, mergedOrigin(lalr, canonicalTarget));
            }
        }
    }
    EXPECT_TRUE(std::ranges::all_of(seen, [](bool value) { return value; }));
}

TEST(LALRkDfaTest, ReportsAConflictIntroducedOnlyByCoreMerging) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e",
            "A -> c", "B -> c",
    });
    const zbik::LRkDfa canonical(grammar, 1);
    const zbik::ParseTable canonicalTable(canonical);
    const zbik::LALRkDfa lalr(canonical);
    const zbik::ParseTable lalrTable(lalr);

    EXPECT_FALSE(canonicalTable.hasConflicts());
    ASSERT_TRUE(lalrTable.hasConflicts());
    EXPECT_TRUE(std::ranges::equal(
            lalrTable.mergeConflicts(), lalrTable.conflicts()));
    EXPECT_TRUE(std::ranges::all_of(lalrTable.mergeConflicts(),
            [](const zbik::Conflict &conflict) { return conflict.reduceReduce(); }));
}

TEST(LALRkDfaTest, SupportsLookaheadGreaterThanOne) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "X -> Y", "X -> b Y a", "Y -> c", "Y -> c a",
    });
    const zbik::ParseTable canonical(zbik::LRkDfa(grammar, 2));
    const zbik::LALRkDfa lalr(grammar, 2);
    const zbik::ParseTable merged(lalr);

    EXPECT_FALSE(canonical.hasConflicts());
    EXPECT_TRUE(merged.hasConflicts());
    EXPECT_FALSE(merged.mergeConflicts().empty());
    EXPECT_LT(lalr.states().size(), lalr.canonical().states().size());
}

TEST(LALRkDfaTest, ExecutesAConflictFreeLalrTwoTable) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A a", "A -> c", "A -> c a",
    });
    const zbik::ParseTable one(zbik::LALRkDfa(grammar, 1));
    const zbik::LALRkDfa twoDfa(grammar, 2);
    const zbik::ParseTable two(twoDfa);
    const zbik::LRMachine machine(two);
    const auto a = grammar.findTerminal("a").value();
    const auto c = grammar.findTerminal("c").value();

    EXPECT_TRUE(one.hasConflicts());
    EXPECT_EQ(twoDfa.states().size(), 6U);
    EXPECT_FALSE(two.hasConflicts());
    EXPECT_TRUE(machine.parse(std::vector{c, a}).accepted);
    EXPECT_TRUE(machine.parse(std::vector{c, a, a}).accepted);
    EXPECT_FALSE(machine.parse(std::vector{c}).accepted);
}

TEST(LALRkDfaTest, DoesNotAttributeACanonicalConflictToMerging) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> E", "E -> E plus E", "E -> id",
    });
    const zbik::LALRkDfa lalr(grammar, 1);
    const zbik::ParseTable table(lalr);

    EXPECT_TRUE(table.hasConflicts());
    EXPECT_TRUE(table.mergeConflicts().empty());
}

TEST(LALRkDfaTest, ReportsNewPairsInsideAnAlreadyConflictingCell) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e",
            "A -> c", "A -> c", "B -> c",
    });
    const zbik::LALRkDfa lalr(grammar, 1);
    const zbik::ParseTable canonical(lalr.canonical()), table(lalr);
    ASSERT_EQ(canonical.conflicts().size(), 2U);
    ASSERT_EQ(table.mergeConflicts().size(), 2U);
    for (const auto &conflict: table.mergeConflicts()) {
        EXPECT_EQ(conflict.reductionRules(),
                  (std::vector<zbik::RuleId>{{4}, {5}, {6}}));
    }
}

TEST(LALRkDfaTest, RemapsShiftTargetsBeforeComparingInheritedPairs) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> E", "E -> E plus E", "E -> open E close", "E -> id",
    });
    const zbik::LALRkDfa lalr(grammar, 1);
    const zbik::ParseTable canonical(lalr.canonical()), table(lalr);
    ASSERT_TRUE(canonical.hasConflicts());
    ASSERT_TRUE(table.hasConflicts());
    EXPECT_TRUE(table.mergeConflicts().empty());
    bool changedTarget = false;
    for (const auto &conflict: canonical.conflicts()) {
        for (const auto &action: conflict.actions()) {
            if (const auto shift = std::get_if<zbik::Shift>(&action)) {
                changedTarget |= mergedOrigin(lalr, shift->target) != shift->target;
            }
        }
    }
    EXPECT_TRUE(changedTarget);
}

TEST(LALRkDfaTest, IsIndependentOfCanonicalHashCollisions) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> C C", "C -> c C", "C -> d",
    });
    const zbik::LALRkDfa normal(grammar, 2);
    const zbik::LALRkDfa collided(
            grammar, 2, [](const zbik::ItemSet &) { return 0U; });

    EXPECT_TRUE(std::ranges::equal(normal.states(), collided.states()));
    EXPECT_TRUE(std::ranges::equal(normal.originGroups(), collided.originGroups()));
}

TEST(LALRkDfaTest, DirectConstructionMatchesCanonicalCoreMerging) {
    const std::vector<std::vector<std::string>> fixtures{
        {"S -> C C", "C -> c C", "C -> d"},
        {"X -> Y", "X -> b Y a", "Y -> c", "Y -> c a"},
        {"S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e",
         "A -> c", "B -> c"},
        {"S -> A marker Dead", "A -> value", "Dead -> Dead"},
        {"S -> a A c", "S -> b A d", "A -> x A", "A ->"},
    };
    for (std::size_t fixture = 0; fixture < fixtures.size(); ++fixture) {
        const auto grammar = zbik::GrammarBuilder{}.build(fixtures[fixture]);
        for (std::size_t k : {1U, 2U, 3U}) {
            SCOPED_TRACE("fixture=" + std::to_string(fixture) +
                         " k=" + std::to_string(k));
            const zbik::LALRkDfa reference(zbik::LRkDfa(grammar, k));
            const zbik::DirectLALRkDfa direct(grammar, k);
            EXPECT_TRUE(std::ranges::equal(reference.states(), direct.states()));
            EXPECT_EQ(zbik::ParseTable(reference).dump(),
                      zbik::ParseTable(direct).dump());
        }
    }
}

TEST(LALRkDfaTest, DirectConstructionReportsConflictsWithoutGuessingTheirOrigin) {
    const auto grammar = zbik::GrammarBuilder{}.build({
        "S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e",
        "A -> c", "B -> c",
    });
    const zbik::LALRkDfa reference(zbik::LRkDfa(grammar, 1));
    const zbik::DirectLALRkDfa direct(grammar, 1);
    const zbik::ParseTable referenceTable(reference), directTable(direct);

    ASSERT_FALSE(referenceTable.mergeConflicts().empty());
    EXPECT_EQ(directTable.conflicts().size(), referenceTable.conflicts().size());
    EXPECT_TRUE(directTable.mergeConflicts().empty());
}

TEST(LALRkDfaTest, DirectConstructionValidatesFirstKAndOwnsItsGraph) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const auto other = zbik::GrammarBuilder{}.build({"S -> b"});
    const zbik::FirstKAnalysis wrong(other, 1);
    EXPECT_THROW((void) zbik::DirectLALRkDfa(grammar, wrong), std::invalid_argument);
    EXPECT_THROW((void) zbik::DirectLALRkDfa(grammar, 0), std::invalid_argument);

    const auto build = [] {
        const auto temporary = zbik::GrammarBuilder{}.build({"S -> a"});
        return zbik::DirectLALRkDfa(temporary, 1);
    };
    const zbik::DirectLALRkDfa owned = build();
    EXPECT_EQ(owned.statistics(), (zbik::LRkDfaStats{3, 4, 2}));
    EXPECT_EQ(owned.dump().find("LALR(1) states=3"), 0U);
}
