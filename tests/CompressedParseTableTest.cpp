#include <gtest/gtest.h>

#include "grammar/GrammarBuilder.h"
#include "lr/CompressedParseTable.h"
#include "lr/LALRkDfa.h"
#include "lr/LRMachine.h"

namespace {

bool compressedAccepts(const zbik::ParseTable &table, const std::vector<zbik::TerminalId> &input) {
    const zbik::CompressedParseTable compressed(table);
    std::vector<zbik::StateId> stack{table.start()};
    std::size_t offset = 0;
    for (std::size_t step = 0; step < 1000; ++step) {
        std::vector<zbik::LookaheadSymbol> symbols;
        std::size_t pos = offset;
        while (pos < input.size() && symbols.size() < table.maxLength()) symbols.push_back(input[pos++]);
        if (symbols.size() < table.maxLength()) symbols.push_back(zbik::endOfInput);
        const auto action = compressed.action(stack.back(), zbik::LookaheadWord{symbols});
        if (!action) return false;
        if (const auto shift = std::get_if<zbik::Shift>(&*action)) {
            if (offset == input.size()) return false;
            stack.push_back(shift->target);
            ++offset;
        } else if (const auto reduce = std::get_if<zbik::Reduce>(&*action)) {
            const auto &rule = table.grammar().rule(reduce->rule);
            if (stack.size() <= rule.size()) return false;
            stack.resize(stack.size() - rule.size());
            const auto target = compressed.goTo(stack.back(), rule.lhs());
            if (!target) return false;
            stack.push_back(*target);
        } else return offset == input.size();
    }
    ADD_FAILURE() << "compressed parser did not terminate";
    return false;
}

} // namespace

TEST(CompressedParseTableTest, PreservesActionsAndBoundedLanguageForMultipleLookaheads) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A a", "S -> b A b a", "A -> b", "A ->"});
    for (std::size_t k: {2U, 3U}) {
        const zbik::ParseTable table{zbik::LRkDfa(grammar, k)};
        const zbik::CompressedParseTable compressed(table);
        for (std::size_t i = 0; i < table.stateCount(); ++i) {
            for (const auto &[word, cell]: table.actionRows()[i]) {
                ASSERT_EQ(compressed.action({i}, word), cell.actions().front());
            }
            for (const auto &[symbol, target]: table.gotoRows()[i]) {
                EXPECT_EQ(compressed.goTo({i}, symbol), target);
            }
            EXPECT_FALSE(compressed.action({i}, zbik::LookaheadWord{}));
        }
        for (std::size_t len = 0; len <= 6; ++len) {
            for (std::size_t bits = 0; bits < (1U << len); ++bits) {
                std::vector<zbik::TerminalId> input;
                for (std::size_t i = 0; i < len; ++i) {
                    input.push_back(*grammar.findTerminal((bits & (1U << i)) ? "a" : "b"));
                }
                EXPECT_EQ(compressedAccepts(table, input), zbik::LRMachine(table).parse(input).accepted);
            }
        }
    }
}

TEST(CompressedParseTableTest, KeepsDifferentReductionsAsExceptions) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A d", "S -> b B d", "S -> a B e", "S -> b A e", "A -> c", "B -> c"});
    const zbik::ParseTable table{zbik::LRkDfa(grammar, 1)};
    const zbik::CompressedParseTable compressed(table);
    for (std::size_t i = 0; i < table.stateCount(); ++i) {
        for (const auto &[word, cell]: table.actionRows()[i]) {
            EXPECT_EQ(compressed.action({i}, word), cell.actions().front());
        }
    }
}

TEST(CompressedParseTableTest, EstimatesPackedBytesIncludingMetadataForATinyTable) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::ParseTable table{zbik::LRkDfa(grammar, 1)};
    const auto stats = zbik::CompressedParseTable(table).statistics();
    // Baseline: header + three ACTION counts/entries + three GOTO counts/one entry.
    EXPECT_EQ(stats.uncompressedBytes, 32U + 3U * 8U + 3U * 28U + 3U * 8U + 12U);
    // Compressed: header + state references + three rows/two exceptions + two GOTO rows.
    EXPECT_EQ(stats.compressedBytes, 32U + 3U * 16U + 3U * 24U + 2U * 28U + 2U * 8U + 12U);
}

TEST(CompressedParseTableTest, DumpsDeterministicJson) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::ParseTable table{zbik::LRkDfa(grammar, 1)};
    const zbik::CompressedParseTable compressed(table);

    EXPECT_EQ(compressed.dumpJson(), R"JSON({
  "parser": "LR(1)",
  "startState": 0,
  "actionRows": [
    {
      "entries": [
        {"lookahead": ["a"], "action": {"kind": "shift", "state": 1}}
      ],
      "default": {"kind": "error"}
    },
    {
      "entries": [],
      "default": {"kind": "reduce", "rule": 0}
    },
    {
      "entries": [
        {"lookahead": [{"eof": true}], "action": {"kind": "accept"}}
      ],
      "default": {"kind": "error"}
    }
  ],
  "actionStateRows": [0, 1, 2],
  "gotoRows": [
    [
      {"nonterminal": "S", "state": 2}
    ],
    []
  ],
  "gotoStateRows": [0, 1, 1]
}
)JSON");
}

TEST(CompressedParseTableTest, DumpsFullMultiSymbolLookaheadKeys) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A a", "S -> b A b a", "A -> b", "A ->"});
    const zbik::CompressedParseTable compressed(
            zbik::ParseTable{zbik::LRkDfa(grammar, 2)});
    const std::string text = compressed.dumpJson();

    EXPECT_NE(text.find("\"parser\": \"LR(2)\""), std::string::npos);
    EXPECT_NE(text.find("\"lookahead\": [\"a\", \"a\"]"), std::string::npos);
    EXPECT_NE(text.find("{\"eof\": true}]"), std::string::npos);
    EXPECT_EQ(text, compressed.dumpJson());

    const zbik::CompressedParseTable lalr(
            zbik::ParseTable{zbik::LALRkDfa(grammar, 2)});
    EXPECT_NE(lalr.dumpJson().find("\"parser\": \"LALR(2)\""),
              std::string::npos);
}

TEST(CompressedParseTableTest, DumpsCompactDslWithTheSameTableKindAndRows) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::CompressedParseTable compressed(
            zbik::ParseTable{zbik::LRkDfa(grammar, 1)});

    EXPECT_EQ(compressed.dumpDsl(), R"DSL(compressed-table "LR(1)" {
  start-state 0;

  action-row 0 {
    ["a"] => shift 1;
    any => error;
  }
  action-row 1 {
    any => reduce 0;
  }
  action-row 2 {
    [EOF] => accept;
    any => error;
  }

  action-state-rows [0, 1, 2];

  goto-row 0 {
    "S" => 2;
  }
  goto-row 1 {
  }

  goto-state-rows [0, 1, 1];
}
)DSL");
}

TEST(CompressedParseTableTest, ExportsDollarTerminalSeparatelyFromEndOfInput) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> $"});
    for (std::size_t k: {1U, 2U, 3U}) {
        const zbik::CompressedParseTable compressed(
                zbik::ParseTable{zbik::LRkDfa(grammar, k)});
        const auto dsl = compressed.dumpDsl();
        const auto json = compressed.dumpJson();
        EXPECT_NE(dsl.find(k == 1 ? "[\"$\"] => shift" : "[\"$\", EOF] => shift"),
                  std::string::npos);
        EXPECT_NE(dsl.find("[EOF] => accept"), std::string::npos);
        EXPECT_NE(json.find(k == 1 ? "[\"$\"]" : "[\"$\", {\"eof\": true}]"),
                  std::string::npos);
        EXPECT_NE(json.find("[{\"eof\": true}], \"action\": {\"kind\": \"accept\"}"),
                  std::string::npos);
    }
}

TEST(CompressedParseTableTest, ExportsEofNamedTerminalSeparatelyFromEndOfInput) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> EOF"});
    const zbik::CompressedParseTable compressed(
            zbik::ParseTable{zbik::LRkDfa(grammar, 2)});
    EXPECT_NE(compressed.dumpDsl().find("[\"EOF\", EOF] => shift"), std::string::npos);
    EXPECT_NE(compressed.dumpDsl().find("[EOF] => accept"), std::string::npos);
    EXPECT_NE(compressed.dumpJson().find("[\"EOF\", {\"eof\": true}]"), std::string::npos);
}

TEST(CompressedParseTableTest, SlrExpressionTableCompressesAndAcceptRemainsExplicit) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "E -> E + T", "E -> T", "T -> T * F", "T -> F", "F -> ( E )", "F -> id"});
    zbik::LRkDfaStats stats{};
    const auto table = zbik::ParseTable::slr(grammar, stats);
    ASSERT_FALSE(table.hasConflicts());
    EXPECT_EQ(stats.states, 12U);
    const zbik::CompressedParseTable compressed(table);
    EXPECT_NE(compressed.dumpJson().find("\"parser\": \"SLR\""), std::string::npos);
    EXPECT_LT(compressed.statistics().compressedBytes, compressed.statistics().uncompressedBytes);
    EXPECT_LT(compressed.statistics().uniqueActionRows, table.stateCount());
    for (std::size_t i = 0; i < table.stateCount(); ++i) {
        for (const auto &[word, cell]: table.actionRows()[i]) {
            if (std::holds_alternative<zbik::Accept>(cell.actions().front())) {
                const auto other = compressed.action({i}, zbik::LookaheadWord{{*grammar.findTerminal("id")}});
                EXPECT_FALSE(other && std::holds_alternative<zbik::Accept>(*other));
            }
        }
    }
}

TEST(CompressedParseTableTest, SlrDetectsConflictsInALalrGrammar) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> L = R", "S -> R", "L -> * R", "L -> id", "R -> L"});
    zbik::LRkDfaStats stats{};
    const auto table = zbik::ParseTable::slr(grammar, stats);
    EXPECT_TRUE(table.hasConflicts());
    EXPECT_THROW(static_cast<void>(zbik::CompressedParseTable(table)), std::invalid_argument);
}
