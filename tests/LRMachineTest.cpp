#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lr/LRMachine.h"

namespace {

struct ParserFixture {
    zbik::Grammar grammar;
    zbik::LRkDfa dfa;
    zbik::ParseTable table;
    zbik::LRMachine machine;

    explicit ParserFixture(std::vector<std::string> rules, std::size_t k = 1)
        : grammar(zbik::GrammarBuilder{}.build(rules)),
          dfa(grammar, k), table(dfa), machine(table) {}

    zbik::TerminalId terminal(const std::string &name) const {
        return grammar.findTerminal(name).value();
    }
};

} // namespace

TEST(LRMachineTest, ExecutesShiftReduceAndAccept) {
    const ParserFixture parser({"S -> a"});
    const std::vector input{parser.terminal("a")};
    const zbik::LRParseResult result = parser.machine.parse(input);

    EXPECT_TRUE(result.accepted);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_TRUE(result.trace.empty());
}

TEST(LRMachineTest, HandlesRecursiveAndEpsilonProductions) {
    const ParserFixture parser({"S -> items", "items -> item items", "items ->"});
    const auto item = parser.terminal("item");

    EXPECT_TRUE(parser.machine.parse(std::vector<zbik::TerminalId>{}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{item}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{item, item, item}).accepted);
}

TEST(LRMachineTest, RecognizesTheCanonicalReferenceGrammar) {
    const ParserFixture parser({
            "S -> L equals R", "S -> R", "L -> star R", "L -> id", "R -> L",
    });
    const auto equals = parser.terminal("equals");
    const auto star = parser.terminal("star");
    const auto id = parser.terminal("id");

    EXPECT_TRUE(parser.machine.parse(std::vector{id}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{id, equals, id}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{star, id, equals, id}).accepted);
    EXPECT_FALSE(parser.machine.parse(std::vector{id, equals}).accepted);
    EXPECT_FALSE(parser.machine.parse(std::vector{equals, id}).accepted);
}

TEST(LRMachineTest, ReturnsReadableSyntaxErrorAndExpectedLookaheads) {
    const ParserFixture parser({"S -> a b"});
    const auto a = parser.terminal("a");
    const auto b = parser.terminal("b");
    const zbik::LRParseResult result = parser.machine.parse(std::vector{a});

    ASSERT_FALSE(result.accepted);
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->inputOffset, 1U);
    EXPECT_EQ(result.error->lookahead, (zbik::LookaheadSymbol{zbik::endOfInput}));
    EXPECT_EQ(result.error->lookaheadWord,
              (zbik::LookaheadWord{{zbik::endOfInput}}));
    EXPECT_EQ(result.error->expected, (std::vector{zbik::LookaheadWord{{b}}}));
    EXPECT_NE(result.error->message.find("input position 1"), std::string::npos);
    EXPECT_NE(result.error->message.find("on EOF"), std::string::npos);
    EXPECT_NE(result.error->message.find("expected 'b'"), std::string::npos);
}

TEST(LRMachineTest, RejectsUnknownTerminalIdsWithoutIndexingTheGrammar) {
    const ParserFixture parser({"S -> a"});
    const zbik::TerminalId unknown{99};
    const zbik::LRParseResult result = parser.machine.parse(std::vector{unknown});

    ASSERT_FALSE(result.accepted);
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->inputOffset, 0U);
    EXPECT_EQ(result.error->lookahead, (zbik::LookaheadSymbol{unknown}));
    EXPECT_EQ(result.error->lookaheadWord, (zbik::LookaheadWord{{unknown}}));
    EXPECT_NE(result.error->message.find("terminal 99"), std::string::npos);
}

TEST(LRMachineTest, CapturesStackRemainingInputAndActionBeforeEveryStep) {
    const ParserFixture parser({"S -> a"});
    const auto a = parser.terminal("a");
    const zbik::LRParseResult result = parser.machine.parse(std::vector{a}, true);

    ASSERT_TRUE(result.accepted);
    ASSERT_EQ(result.trace.size(), 3U);
    EXPECT_EQ(result.trace[0], (zbik::LRTraceStep{
            {{0}}, {a, zbik::endOfInput}, zbik::LookaheadWord{{a}}, zbik::Shift{{1}},
    }));
    EXPECT_EQ(result.trace[1], (zbik::LRTraceStep{
            {{0}, {1}}, {zbik::endOfInput},
            zbik::LookaheadWord{{zbik::endOfInput}}, zbik::Reduce{{0}},
    }));
    EXPECT_EQ(result.trace[2], (zbik::LRTraceStep{
            {{0}, {2}}, {zbik::endOfInput},
            zbik::LookaheadWord{{zbik::endOfInput}}, zbik::Accept{},
    }));
}

TEST(LRMachineTest, RefusesAConflictingTableBeforeExecution) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> E", "E -> E plus E", "E -> id",
    });
    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));

    ASSERT_TRUE(table.hasConflicts());
    EXPECT_THROW(static_cast<void>(zbik::LRMachine(table)), std::invalid_argument);
}

TEST(LRMachineTest, ParseTableAndMachineDoNotDependOnTheSourceDfaLifetime) {
    const auto makeTable = [] {
        const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
        return zbik::ParseTable(zbik::LRkDfa(grammar, 1));
    };
    const zbik::ParseTable table = makeTable();
    const zbik::LRMachine machine(table);
    const auto a = table.grammar().findTerminal("a").value();

    EXPECT_TRUE(machine.parse(std::vector{a}).accepted);
}

TEST(LRMachineTest, ExecutesTheReferenceGrammarWithTwoTokenLookahead) {
    const ParserFixture parser({
            "X -> Y", "X -> b Y a", "Y -> c", "Y -> c a",
    }, 2);
    const auto a = parser.terminal("a");
    const auto b = parser.terminal("b");
    const auto c = parser.terminal("c");

    EXPECT_TRUE(parser.machine.parse(std::vector{c}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{c, a}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{b, c, a}).accepted);
    EXPECT_TRUE(parser.machine.parse(std::vector{b, c, a, a}).accepted);
    EXPECT_FALSE(parser.machine.parse(std::vector{b, c}).accepted);
}

TEST(LRMachineTest, AppendsEofWhenInputIsShorterThanK) {
    const ParserFixture parser({"S -> a"}, 3);
    const auto a = parser.terminal("a");
    const zbik::LRParseResult result = parser.machine.parse(std::vector{a}, true);

    ASSERT_TRUE(result.accepted);
    ASSERT_EQ(result.trace.size(), 3U);
    EXPECT_EQ(result.trace[0].lookahead,
              (zbik::LookaheadWord{{a, zbik::endOfInput}}));
    EXPECT_EQ(result.trace[1].lookahead,
              (zbik::LookaheadWord{{zbik::endOfInput}}));
    EXPECT_EQ(result.trace[2].lookahead,
              (zbik::LookaheadWord{{zbik::endOfInput}}));
}

TEST(LRMachineTest, DoesNotMatchAnEofTerminatedActionByPrefix) {
    const ParserFixture parser({"S -> a", "Unused -> b"}, 2);
    const auto a = parser.terminal("a");
    const auto b = parser.terminal("b");
    const zbik::LRParseResult result = parser.machine.parse(std::vector{a, b});

    ASSERT_FALSE(result.accepted);
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->inputOffset, 0U);
    EXPECT_EQ(result.error->lookaheadWord, (zbik::LookaheadWord{{a, b}}));
    EXPECT_EQ(result.error->expected,
              (std::vector{zbik::LookaheadWord{{a, zbik::endOfInput}}}));
}
