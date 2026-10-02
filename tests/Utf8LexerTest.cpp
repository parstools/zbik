#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "lexer/ByteLexer.h"
#include "lexer/LexerAutomaton.h"
#include "lexer/Utf8Lexer.h"
#include "regex/RegexParser.h"
#include "regex/UnicodeProperties.h"

namespace {

TEST(Utf8LexerTest, IncrementalClassesPreserveUnicodeOffsetsAndMaskedSkipRules) {
    const zbik::TerminalId pair{0}, single{1}, marker{2};
    const zbik::Utf8Lexer lexer({
        {pair, "'ąą'", 1}, {single, "'ą'"},
        {std::nullopt, "'!'", 2}, {marker, "'!'"},
    });
    const std::string source = "ąą!ą";
    std::size_t offset = 0;
    EXPECT_EQ(lexer.next(source, offset, 1), (zbik::LexedToken{pair, 0, "ąą"}));
    EXPECT_EQ(offset, 4U);
    EXPECT_EQ(lexer.next(source, offset, 2), (zbik::LexedToken{single, 5, "ą"}));
    EXPECT_EQ(offset, 7U);
    EXPECT_FALSE(lexer.next(source, offset, 0));
    EXPECT_EQ(lexer.tokenize(source).terminalIds, (std::vector{single, single, marker, single}));
}

TEST(Utf8LexerTest, StructuredLazyRulesHonorClassesInBatchAndIncrementalCalls) {
    const zbik::TerminalId word{0};
    const zbik::RegexParser parser;
    const auto comment = zbik::RegexAst::concatenate({
        parser.parse("'/*'"),
        zbik::RegexAst::repeat(parser.parse("[^\\n]"), zbik::RegexQuantifier::LazyZeroOrMore),
        parser.parse("'*/'"),
    });
    const zbik::Utf8Lexer lexer({{std::nullopt, {}, 1}, {word, {}}},
                               {comment, parser.parse("[a-z]+")});
    const std::string source = "/*a*/read";
    EXPECT_EQ(lexer.tokenize(source, 1).tokens, (std::vector<zbik::LexedToken>{{word, 5, "read"}}));
    std::size_t offset = 0;
    EXPECT_EQ(lexer.next(source, offset, 1), (zbik::LexedToken{word, 5, "read"}));
    offset = 0;
    EXPECT_THROW((void)lexer.next(source, offset, 0), zbik::Utf8LexerError);
}

TEST(Utf8LexerTest, TokenizesUnicodeScalarsAndKeepsByteOffsets) {
    const zbik::TerminalId word{0};
    const zbik::TerminalId emoji{1};
    const zbik::Utf8Lexer lexer({
            {word, "[a-ząśćżźół]+"},
            {emoji, "'😀'"},
            {std::nullopt, "[ \\t\\n]+"},
    });

    const zbik::LexResult result = lexer.tokenize("zażółć 😀 ą");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
                                     {word, 0, "zażółć"},
                                     {emoji, 11, "😀"},
                                     {word, 16, "ą"},
                             }));
}

TEST(Utf8LexerTest, SupportsUnicodeEscapesAndNegatedClasses) {
    const zbik::TerminalId letter{0};
    const zbik::TerminalId other{1};
    const zbik::Utf8Lexer lexer({
            {letter, "[\\u0100-\\u017F]+"},
            {other, "[^\\u0100-\\u017F]+"},
    });

    const zbik::LexResult result = lexer.tokenize("ĄĆabc😀");
    ASSERT_EQ(result.tokens.size(), 2U);
    EXPECT_EQ(result.tokens[0], (zbik::LexedToken{letter, 0, "ĄĆ"}));
    EXPECT_EQ(result.tokens[1], (zbik::LexedToken{other, 4, "abc😀"}));
}

TEST(Utf8LexerTest, RejectsInvalidUtf8WithItsByteOffset) {
    const zbik::Utf8Lexer lexer({{zbik::TerminalId{0}, "[a-z]+"}});
    const std::string input{"a\xC0\xAF", 3};
    try {
        (void) lexer.tokenize(input);
        FAIL() << "Expected Utf8LexerError";
    } catch (const zbik::Utf8LexerError &error) {
        EXPECT_EQ(error.kind(), zbik::Utf8LexerErrorKind::InvalidEncoding);
        EXPECT_EQ(error.tokenStart(), 1U);
        EXPECT_EQ(error.errorOffset(), 1U);
    }
}

TEST(Utf8LexerTest, TokenizesUnicodeIdentifiersFromXidProperties) {
    const zbik::TerminalId identifier{0};
    const zbik::Utf8Lexer lexer({
            {identifier, "[\\p{XID_Start}_][\\p{XID_Continue}_]*"},
            {std::nullopt, "[ ]+"},
    });
    const zbik::LexResult result = lexer.tokenize("nazwa ą2 Δx");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
                                     {identifier, 0, "nazwa"},
                                     {identifier, 6, "ą2"},
                                     {identifier, 10, "Δx"},
                             }));
    EXPECT_FALSE(zbik::unicodeDataVersion().empty());
    EXPECT_LT(lexer.transitionRangeCount(), 5000U);
}

TEST(Utf8LexerTest, ByteLexerRejectsNonBytePatterns) {
    EXPECT_THROW((zbik::ByteLexer({{zbik::TerminalId{0}, "'ą'"}})), zbik::LexerBuildError);
}

TEST(Utf8LexerTest, BuildsTheSameAutomatonFromStructuredRegexTrees) {
    const std::vector<zbik::LexerRule> rules{
            {zbik::TerminalId{0}, "[a-z]+"},
            {std::nullopt, "[ ]+"},
    };
    const zbik::RegexParser parser;
    const std::vector<zbik::RegexAst> expressions{
            parser.parse(rules[0].pattern),
            parser.parse(rules[1].pattern),
    };
    const zbik::LexerAutomaton text(rules);
    const zbik::LexerAutomaton structured(expressions);

    ASSERT_EQ(structured.stateCount(), text.stateCount());
    EXPECT_EQ(structured.transitionRangeCount(), text.transitionRangeCount());
    for (std::size_t state = 0; state < text.stateCount(); ++state) {
        EXPECT_EQ(structured.acceptingRule(state), text.acceptingRule(state));
        for (const zbik::CodePoint codePoint: std::array<zbik::CodePoint, 5>{' ', 'a', 'z', '0', 0x0105U}) {
            EXPECT_EQ(structured.nextState(state, codePoint), text.nextState(state, codePoint));
        }
    }
}

TEST(Utf8LexerTest, TokenizesWithMetadataAndStructuredRegexTrees) {
    const zbik::TerminalId identifier{7};
    const std::vector<zbik::LexerRule> rules{
            {identifier, {}},
            {std::nullopt, {}},
    };
    const zbik::RegexParser parser;
    const zbik::Utf8Lexer lexer(rules, {parser.parse("[a-z]+"), parser.parse("[ ]+")});

    const zbik::LexResult result = lexer.tokenize("one two");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
                                     {identifier, 0, "one"},
                                     {identifier, 4, "two"},
                             }));
    EXPECT_EQ(result.terminalIds, (std::vector<zbik::TerminalId>{identifier, identifier}));
    EXPECT_THROW((zbik::Utf8Lexer(rules, {parser.parse("[a-z]+")})), zbik::LexerBuildError);
}

TEST(Utf8LexerTest, StopsLazyRulesAtTheirFirstPreferredTerminator) {
    const zbik::RegexParser parser;
    const zbik::RegexAst comment = zbik::RegexAst::concatenate({
            parser.parse("'/*'"),
            zbik::RegexAst::repeat(zbik::RegexAst::codePointClass(zbik::CodePointClass({
                                           {0, zbik::maxUnicodeCodePoint},
                                   })),
                                   zbik::RegexQuantifier::LazyZeroOrMore),
            parser.parse("'*/'"),
    });
    const zbik::TerminalId identifier{4};
    const zbik::Utf8Lexer lexer({{std::nullopt, {}}, {identifier, {}}}, {comment, parser.parse("[a-z]+")});

    const zbik::LexResult result = lexer.tokenize("/* first */middle/* second */");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
                                     {identifier, 11, "middle"},
                             }));
}

TEST(Utf8LexerTest, KeepsMaximalMunchBetweenLazyAndGreedyRules) {
    const zbik::RegexParser parser;
    const zbik::RegexAst lazy = zbik::RegexAst::concatenate({
            zbik::RegexAst::repeat(parser.parse("a"), zbik::RegexQuantifier::LazyZeroOrMore),
            parser.parse("a"),
    });
    const zbik::TerminalId shortRule{1};
    const zbik::TerminalId longRule{2};
    const zbik::Utf8Lexer lexer({{shortRule, {}}, {longRule, {}}},
                                {lazy, zbik::RegexAst::repeat(parser.parse("a"), zbik::RegexQuantifier::OneOrMore)});

    const zbik::LexResult result = lexer.tokenize("aaa");
    EXPECT_EQ(result.tokens, (std::vector<zbik::LexedToken>{
                                     {longRule, 0, "aaa"},
                             }));
}

} // namespace
