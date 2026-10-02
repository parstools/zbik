#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lr/ParseTable.h"

namespace {

zbik::LookaheadWord terminal(zbik::TerminalId id) {
    return zbik::LookaheadWord{{id}};
}

zbik::LookaheadWord eof() {
    return zbik::LookaheadWord{{zbik::endOfInput}};
}

} // namespace

TEST(ParseTableTest, BuildsTheCompleteTableForASingleProduction) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRkDfa dfa(grammar, 1);
    const zbik::ParseTable table(dfa);
    const auto a = grammar.findTerminal("a").value();

    ASSERT_EQ(table.stateCount(), 3U);
    ASSERT_EQ(table.actionRows().size(), 3U);
    ASSERT_EQ(table.gotoRows().size(), 3U);
    EXPECT_EQ(table.actionRows()[0], (zbik::ActionRow{
            {terminal(a), zbik::ActionCell{zbik::Shift{{1}}}},
    }));
    EXPECT_EQ(table.gotoRows()[0], (zbik::GotoRow{
            {grammar.start(), zbik::StateId{2}},
    }));
    EXPECT_EQ(table.actionRows()[1], (zbik::ActionRow{
            {eof(), zbik::ActionCell{zbik::Reduce{{0}}}},
    }));
    EXPECT_TRUE(table.gotoRows()[1].empty());
    EXPECT_EQ(table.actionRows()[2], (zbik::ActionRow{
            {eof(), zbik::ActionCell{zbik::Accept{}}},
    }));
    EXPECT_TRUE(table.gotoRows()[2].empty());
    EXPECT_FALSE(table.hasConflicts());
    EXPECT_TRUE(table.conflicts().empty());
}

TEST(ParseTableTest, KeepsTerminalAndNonterminalKeySpacesSeparate) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A", "A -> a"});
    const zbik::LRkDfa dfa(grammar, 1);
    const zbik::ParseTable table(dfa);
    const auto a = grammar.findTerminal("a").value();

    ASSERT_EQ(a.value, grammar.start().value);
    EXPECT_TRUE(table.actions({0}, terminal(a)).contains(zbik::Shift{{1}}));
    ASSERT_TRUE(table.goTo({0}, grammar.start()).has_value());
    EXPECT_EQ(table.goTo({0}, grammar.start()), zbik::StateId{2});
    EXPECT_EQ(table.goTo({0}, grammar.findNonterminal("A").value()), zbik::StateId{3});
    EXPECT_FALSE(table.goTo({1}, grammar.start()).has_value());
}

TEST(ParseTableTest, PlacesEpsilonReductionWithoutCreatingAnEpsilonActionKey) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S ->"});
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));

    EXPECT_TRUE(table.actions({0}, eof()).contains(zbik::Reduce{{0}}));
    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{}).empty());
    EXPECT_EQ(table.goTo({0}, grammar.start()), zbik::StateId{1});
    EXPECT_TRUE(table.actions({1}, eof()).contains(zbik::Accept{}));
}

TEST(ParseTableTest, RetainsAndReportsReduceReduceConflicts) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a", "S -> a"});
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));
    const auto a = grammar.findTerminal("a").value();
    const zbik::ActionCell &cell = table.actions({1}, eof());

    EXPECT_TRUE(table.actions({0}, terminal(a)).contains(zbik::Shift{{1}}));
    EXPECT_EQ(cell, (zbik::ActionCell{zbik::Reduce{{0}}, zbik::Reduce{{1}}}));
    ASSERT_EQ(table.conflicts().size(), 1U);
    EXPECT_EQ(table.conflicts()[0].state(), (zbik::StateId{1}));
    EXPECT_EQ(table.conflicts()[0].lookahead(), eof());
    EXPECT_TRUE(table.conflicts()[0].reduceReduce());
}

TEST(ParseTableTest, RetainsAndReportsShiftReduceConflicts) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> E", "E -> E plus E", "E -> id",
    });
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));

    ASSERT_EQ(table.conflicts().size(), 1U);
    const zbik::Conflict &conflict = table.conflicts()[0];
    EXPECT_TRUE(conflict.shiftReduce());
    EXPECT_FALSE(conflict.reduceReduce());
    EXPECT_EQ(conflict.lookahead(), terminal(grammar.findTerminal("plus").value()));
    EXPECT_EQ(conflict.actions().size(), 2U);
}

TEST(ParseTableTest, DumpsRowsInAStableOrder) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));

    EXPECT_EQ(table.dump(),
            "LR(1) table states=3 conflicts=0\n"
            "state 0:\n"
            "  ACTION [t0] = {shift 1}\n"
            "  GOTO N0 = 2\n"
            "state 1:\n"
            "  ACTION [$] = {reduce R0}\n"
            "state 2:\n"
            "  ACTION [$] = {accept}\n");
}

TEST(ParseTableTest, BuildsShiftKeysFromTheFullFirstKContext) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A d", "A -> b", "A ->",
    });
    const auto a = grammar.findTerminal("a").value();
    const auto b = grammar.findTerminal("b").value();
    const auto d = grammar.findTerminal("d").value();
    const zbik::LRkDfa dfa(grammar, 2);
    const zbik::ParseTable table(dfa);
    const auto target = dfa.state(dfa.start()).transitions.at(zbik::SymbolRef{a});

    EXPECT_EQ(table.maxLength(), 2U);
    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{{a, b}})
                        .contains(zbik::Shift{target}));
    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{{a, d}})
                        .contains(zbik::Shift{target}));
    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{{a}}).empty());
    EXPECT_EQ(table.actionRows()[0].size(), 2U);
    EXPECT_EQ(table.actionTrieNodeCount({0}), 4U); // root, shared a, then b and d
}

TEST(ParseTableTest, UsesEofTerminatedPrefixesWhenInputCanEndBeforeK) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const auto a = grammar.findTerminal("a").value();
    const zbik::LRkDfa dfa(grammar, 3);
    const zbik::ParseTable table(dfa);
    const auto target = dfa.state(dfa.start()).transitions.at(zbik::SymbolRef{a});

    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{{a, zbik::endOfInput}})
                        .contains(zbik::Shift{target}));
    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{{a}}).empty());
}

TEST(ParseTableTest, PreservesLR1ShiftTransitionsForANonproductiveSuffix) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a Dead", "Dead -> Dead",
    });
    const auto a = grammar.findTerminal("a").value();
    const zbik::LRkDfa dfa(grammar, 1);
    const zbik::ParseTable table(dfa);
    const auto target = dfa.state(dfa.start()).transitions.at(zbik::SymbolRef{a});

    EXPECT_TRUE(table.actions({0}, zbik::LookaheadWord{{a}})
                        .contains(zbik::Shift{target}));
}

TEST(ParseTableTest, ResolvesTheReferenceGrammarAtKTwo) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "X -> Y", "X -> b Y a", "Y -> c", "Y -> c a",
    });

    const zbik::ParseTable one(zbik::LRkDfa(grammar, 1));
    const zbik::ParseTable two(zbik::LRkDfa(grammar, 2));

    EXPECT_TRUE(one.hasConflicts());
    EXPECT_FALSE(two.hasConflicts());
}

TEST(ParseTableTest, DoesNotEnumerateTheAlphabetPowerForLargeK) {
    std::vector<std::string> rules{"S -> t0"};
    for (std::size_t i = 1; i < 64; ++i) {
        rules.push_back("Unused" + std::to_string(i) + " -> t" + std::to_string(i));
    }
    const auto grammar = zbik::GrammarBuilder{}.build(rules);
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 32));

    EXPECT_EQ(table.stateCount(), 3U);
    EXPECT_EQ(table.actionRows()[0].size(), 1U);
    EXPECT_FALSE(table.hasConflicts());
}
