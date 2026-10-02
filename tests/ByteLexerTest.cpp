#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lexer/ByteLexer.h"
#include "lr/LRMachine.h"

namespace {

TEST(ByteLexerTest, ClassesRetainShorterTokensAndLowerPriorityCandidatesInOneDfa) {
    constexpr zbik::LexerClassMask high = zbik::LexerClassMask{1} << 63;
    const zbik::TerminalId shr{0}, gt{1}, keyword{2}, identifier{3};
    const zbik::ByteLexer lexer({
        {shr, "'>>'", high}, {gt, "'>'"}, {keyword, "'read'", 1},
        {identifier, "[a-z]+"}, {std::nullopt, "[ ]+"},
    });
    const auto states = lexer.dfaStateCount();
    EXPECT_EQ(lexer.tokenize("read>>", 0).terminalIds, (std::vector{identifier, gt, gt}));
    EXPECT_EQ(lexer.tokenize("read>>", high | 1).terminalIds, (std::vector{keyword, shr}));
    EXPECT_EQ(lexer.tokenize("reader>>", high | 1).terminalIds, (std::vector{identifier, shr}));
    std::size_t offset = 0;
    const auto first = lexer.next(">>>", offset, high);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->text, ">>");
    EXPECT_EQ(offset, 2U);
    EXPECT_EQ(lexer.next(">>>", offset, 0)->text, ">");
    EXPECT_EQ(offset, 3U);
    EXPECT_FALSE(lexer.next(">>>", offset, high));
    EXPECT_EQ(lexer.dfaStateCount(), states);
    offset = 4;
    EXPECT_THROW((void)lexer.next(">>>", offset, 0), std::out_of_range);
}

TEST(ByteLexerTest, UsesLongestMatchBeforeRulePriority) {
    const zbik::TerminalId equal{0};
    const zbik::TerminalId equalEqual{1};
    const zbik::ByteLexer lexer({
            {equal, "="},
            {equalEqual, "=="},
    });
    const zbik::LexResult result = lexer.tokenize("===");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
            {equalEqual, 0, "=="},
            {equal, 2, "="},
    }));
    EXPECT_EQ(result.terminalIds, (std::vector{equalEqual, equal}));
}

TEST(ByteLexerTest, UsesEarlierRuleToBreakEqualLengthTies) {
    const zbik::TerminalId keyword{0};
    const zbik::TerminalId identifier{1};
    const zbik::ByteLexer lexer({
            {keyword, "if"},
            {identifier, "[a-z]+"},
            {std::nullopt, "[ ]+"},
    });
    const zbik::LexResult result = lexer.tokenize("if ifx");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
            {keyword, 0, "if"},
            {identifier, 3, "ifx"},
    }));
}

TEST(ByteLexerTest, SkipsRulesWithoutLosingByteOffsets) {
    const zbik::TerminalId word{0};
    const zbik::ByteLexer lexer({
            {std::nullopt, "[ \\t\\n]+"},
            {word, "[a-z]+"},
    });
    const zbik::LexResult result = lexer.tokenize(" \nalpha\tbeta");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
            {word, 2, "alpha"},
            {word, 8, "beta"},
    }));
    const zbik::LexResult skipped = lexer.tokenize(" \t\n");
    EXPECT_TRUE(skipped.tokens.empty());
    EXPECT_TRUE(skipped.terminalIds.empty());
    EXPECT_GT(lexer.dfaStateCount(), 1U);
}

TEST(ByteLexerTest, ReportsUnknownByteAndIncompleteTokenPrecisely) {
    const zbik::ByteLexer lexer({{zbik::TerminalId{0}, "ab"}});
    try {
        (void) lexer.tokenize("ac");
        FAIL() << "Expected LexerError";
    } catch (const zbik::LexerError &error) {
        EXPECT_EQ(error.tokenStart(), 0U);
        EXPECT_EQ(error.errorOffset(), 1U);
        EXPECT_EQ(error.byte(), std::optional<std::uint8_t>('c'));
    }

    try {
        (void) lexer.tokenize("a");
        FAIL() << "Expected LexerError";
    } catch (const zbik::LexerError &error) {
        EXPECT_EQ(error.tokenStart(), 0U);
        EXPECT_EQ(error.errorOffset(), 1U);
        EXPECT_FALSE(error.byte().has_value());
    }
}

TEST(ByteLexerTest, RejectsEmptyRuleSetsNullableRulesAndBadRegexes) {
    EXPECT_THROW((zbik::ByteLexer({})), zbik::LexerBuildError);
    for (const std::string pattern: {"", "a*", "a?", "a|"}) {
        try {
            (void) zbik::ByteLexer({{zbik::TerminalId{0}, pattern}});
            FAIL() << "Expected LexerBuildError for " << pattern;
        } catch (const zbik::LexerBuildError &error) {
            EXPECT_EQ(error.ruleIndex(), std::optional<std::size_t>(0));
        }
    }
    try {
        (void) zbik::ByteLexer({{zbik::TerminalId{0}, "[z-a]"}});
        FAIL() << "Expected LexerBuildError";
    } catch (const zbik::LexerBuildError &error) {
        EXPECT_EQ(error.ruleIndex(), std::optional<std::size_t>(0));
        EXPECT_NE(std::string(error.what()).find("Descending byte range"),
                  std::string::npos);
    }
}

TEST(ByteLexerTest, TokenizesNulAndHighBytes) {
    const zbik::TerminalId binary{0};
    const zbik::ByteLexer lexer({{binary, "\\x00[\\x80-\\xFF]+"}});
    const std::string input{"\0\x80\xFF", 3};
    const zbik::LexResult result = lexer.tokenize(input);
    ASSERT_EQ(result.tokens.size(), 1U);
    EXPECT_EQ(result.tokens.front().offset, 0U);
    EXPECT_EQ(result.tokens.front().text, input);
}

TEST(ByteLexerTest, ComplementsClassesWithinTheByteAlphabet) {
    const zbik::TerminalId other{0};
    const zbik::ByteLexer lexer({{other, "[^a]+"}});
    const std::string input{"\0\xFF", 2};
    EXPECT_EQ(lexer.tokenize(input).tokens,
              (std::vector<zbik::LexedToken>{{other, 0, input}}));
    EXPECT_THROW((void) lexer.tokenize("a"), zbik::LexerError);
}

TEST(ByteLexerTest, FeedsTerminalIdsDirectlyIntoTheLrMachine) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> kw id equals number",
    });
    const auto kw = grammar.findTerminal("kw").value();
    const auto id = grammar.findTerminal("id").value();
    const auto equals = grammar.findTerminal("equals").value();
    const auto number = grammar.findTerminal("number").value();
    const zbik::ByteLexer lexer({
            {kw, "if"},
            {id, "[a-z]+"},
            {equals, "="},
            {number, "[0-9]+"},
            {std::nullopt, "[ \\t\\n]+"},
    });
    const zbik::LexResult lexed = lexer.tokenize("if name=123");

    const zbik::ParseTable table(zbik::LRkDfa(grammar, 1));
    const zbik::LRMachine parser(table);
    EXPECT_TRUE(parser.parse(lexed.terminalIds).accepted);
    EXPECT_EQ(lexed.tokens, (std::vector<zbik::LexedToken>{
            {kw, 0, "if"}, {id, 3, "name"},
            {equals, 7, "="}, {number, 8, "123"},
    }));
}

} // namespace
