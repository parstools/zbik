#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "regex/RegexParser.h"

namespace {

std::uint8_t onlyByte(const zbik::RegexAst &expression) {
    EXPECT_EQ(expression.kind(), zbik::RegexAst::Kind::ByteClass);
    EXPECT_EQ(expression.bytes().ranges().size(), 1U);
    EXPECT_EQ(expression.bytes().ranges().front().first,
              expression.bytes().ranges().front().last);
    return expression.bytes().ranges().front().first;
}

TEST(RegexParserTest, PreservesConcatenationAndQuantifierFromZubr) {
    const auto expression = zbik::RegexParser{}.parse("a*bb");
    ASSERT_EQ(expression.kind(), zbik::RegexAst::Kind::Concatenation);
    ASSERT_EQ(expression.elements().size(), 3U);
    EXPECT_EQ(expression.elements()[0].kind(), zbik::RegexAst::Kind::Repetition);
    EXPECT_EQ(expression.elements()[0].quantifier(), zbik::RegexQuantifier::ZeroOrMore);
    EXPECT_EQ(onlyByte(expression.elements()[0].repeated()), 'a');
    EXPECT_EQ(onlyByte(expression.elements()[1]), 'b');
    EXPECT_EQ(onlyByte(expression.elements()[2]), 'b');
}

TEST(RegexParserTest, AppliesTheExpectedPrecedence) {
    const auto expression = zbik::RegexParser{}.parse("(a|b)*ab+bc?");
    ASSERT_EQ(expression.kind(), zbik::RegexAst::Kind::Concatenation);
    ASSERT_EQ(expression.elements().size(), 5U);
    ASSERT_EQ(expression.elements()[0].kind(), zbik::RegexAst::Kind::Repetition);
    EXPECT_EQ(expression.elements()[0].repeated().kind(), zbik::RegexAst::Kind::Alternation);
    EXPECT_EQ(expression.elements()[2].quantifier(), zbik::RegexQuantifier::OneOrMore);
    EXPECT_EQ(expression.elements()[4].quantifier(), zbik::RegexQuantifier::ZeroOrOne);

    const auto topLevel = zbik::RegexParser{}.parse("ab|c");
    ASSERT_EQ(topLevel.kind(), zbik::RegexAst::Kind::Alternation);
    EXPECT_EQ(topLevel.alternatives().size(), 2U);
    EXPECT_EQ(topLevel.alternatives()[0].kind(), zbik::RegexAst::Kind::Concatenation);
}

TEST(RegexParserTest, FlattensRedundantConcatenationGroups) {
    const std::pair<const char *, std::size_t> oldZubrCases[] = {
            {"a(bcdef)g", 7},
            {"a(b)c", 3},
            {"ab(cd)a", 5},
            {"ab()dc(c)a(bc)", 8},
            {"((abcdef))", 6},
    };
    for (const auto &[pattern, expectedSize]: oldZubrCases) {
        const auto oldCase = zbik::RegexParser{}.parse(pattern);
        ASSERT_EQ(oldCase.kind(), zbik::RegexAst::Kind::Concatenation)
                << pattern;
        EXPECT_EQ(oldCase.elements().size(), expectedSize) << pattern;
    }

    const auto expression = zbik::RegexParser{}.parse("a(bcdef)g(a|b)c");
    ASSERT_EQ(expression.kind(), zbik::RegexAst::Kind::Concatenation);
    ASSERT_EQ(expression.elements().size(), 9U);
    EXPECT_EQ(expression.elements()[7].kind(), zbik::RegexAst::Kind::Alternation);
}

TEST(RegexParserTest, RepresentsByteClassesAsCanonicalRanges) {
    const auto expression = zbik::RegexParser{}.parse("[a-cb-dx]");
    ASSERT_EQ(expression.kind(), zbik::RegexAst::Kind::ByteClass);
    ASSERT_EQ(expression.bytes().ranges().size(), 2U);
    EXPECT_EQ(expression.bytes().ranges()[0], (zbik::ByteRange{'a', 'd'}));
    EXPECT_EQ(expression.bytes().ranges()[1], (zbik::ByteRange{'x', 'x'}));
}

TEST(RegexParserTest, ComplementsNegatedClassesOverUnicodeScalars) {
    const auto expression = zbik::RegexParser{}.parse("[^a-c]");
    ASSERT_EQ(expression.bytes().ranges().size(), 3U);
    EXPECT_EQ(expression.bytes().ranges()[0], (zbik::ByteRange{0, 'a' - 1}));
    EXPECT_EQ(expression.bytes().ranges()[1],
              (zbik::ByteRange{'c' + 1, zbik::firstHighSurrogate - 1}));
    EXPECT_EQ(expression.bytes().ranges()[2],
              (zbik::ByteRange{zbik::lastLowSurrogate + 1,
                               zbik::maxUnicodeCodePoint}));
    EXPECT_FALSE(expression.bytes().contains('b'));
    EXPECT_TRUE(expression.bytes().contains(0x1F600));

    const auto emptyLanguage =
            zbik::RegexParser{}.parse("[^\\x00-\\U0010FFFF]");
    EXPECT_EQ(emptyLanguage.kind(), zbik::RegexAst::Kind::ByteClass);
    EXPECT_TRUE(emptyLanguage.bytes().ranges().empty());
    EXPECT_EQ(zbik::RegexParser{}.parse("").kind(), zbik::RegexAst::Kind::Epsilon);
}

TEST(RegexParserTest, ParsesEscapesAndQuotedByteSequences) {
    const auto expression = zbik::RegexParser{}.parse("'a|b'\\x00\\n");
    ASSERT_EQ(expression.kind(), zbik::RegexAst::Kind::Concatenation);
    ASSERT_EQ(expression.elements().size(), 5U);
    EXPECT_EQ(onlyByte(expression.elements()[1]), '|');
    EXPECT_EQ(onlyByte(expression.elements()[3]), 0);
    EXPECT_EQ(onlyByte(expression.elements()[4]), '\n');
}

TEST(RegexParserTest, ExpandsPinnedUnicodeIdentifierProperties) {
    const auto start =
            zbik::RegexParser{}.parse("[\\p{XID_Start}_]");
    EXPECT_TRUE(start.codePoints().contains('A'));
    EXPECT_TRUE(start.codePoints().contains(0x0105));
    EXPECT_TRUE(start.codePoints().contains('_'));
    EXPECT_FALSE(start.codePoints().contains('1'));

    const auto continuation =
            zbik::RegexParser{}.parse("\\p{XID_Continue}");
    EXPECT_TRUE(continuation.codePoints().contains('1'));
    EXPECT_TRUE(continuation.codePoints().contains(0x0301));
    EXPECT_THROW((void) zbik::RegexParser{}.parse("\\p{Unknown}"),
                 zbik::RegexParseError);
}

TEST(RegexParserTest, SupportsExplicitEpsilonAlternatives) {
    const auto expression = zbik::RegexParser{}.parse("a|");
    ASSERT_EQ(expression.kind(), zbik::RegexAst::Kind::Alternation);
    ASSERT_EQ(expression.alternatives().size(), 2U);
    EXPECT_EQ(expression.alternatives()[1].kind(), zbik::RegexAst::Kind::Epsilon);
    EXPECT_EQ(zbik::RegexParser{}.parse("()").kind(), zbik::RegexAst::Kind::Epsilon);
}

TEST(RegexParserTest, OwnsItsTreeAndCopiesItDeeply) {
    zbik::RegexAst original = zbik::RegexParser{}.parse("(a|b)+c");
    const zbik::RegexAst copy = original;
    original = zbik::RegexParser{}.parse("x");
    EXPECT_EQ(copy.kind(), zbik::RegexAst::Kind::Concatenation);
    EXPECT_EQ(copy.elements().front().kind(), zbik::RegexAst::Kind::Repetition);
    EXPECT_EQ(copy.elements().front().repeated().kind(), zbik::RegexAst::Kind::Alternation);
}

TEST(RegexParserTest, ReportsMalformedPatternsWithByteOffsets) {
    const auto expectOffset = [](std::string pattern, std::size_t offset) {
        try {
            (void) zbik::RegexParser{}.parse(pattern);
            FAIL() << "Expected RegexParseError for " << pattern;
        } catch (const zbik::RegexParseError &error) {
            EXPECT_EQ(error.offset(), offset);
            EXPECT_NE(std::string(error.what()).find("byte"), std::string::npos);
        }
    };

    expectOffset("(ab", 3);
    expectOffset("a**", 2);
    expectOffset("[z-a]", 4);
    expectOffset("\\xG0", 0);
    expectOffset("'abc", 4);
}

} // namespace
