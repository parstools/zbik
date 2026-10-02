#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "regex/RegexNfaMatcher.h"
#include "regex/RegexParser.h"

namespace {

zbik::RegexNfa build(std::string_view pattern) {
    return zbik::RegexNfa::fromRegex(zbik::RegexParser{}.parse(pattern));
}

bool matches(const zbik::RegexNfa &nfa, std::string_view input) {
    return zbik::RegexNfaMatcher::matches(nfa, input);
}

TEST(RegexNfaMatcherTest, MatchesLiteralConcatenationAndByteClasses) {
    const auto nfa = build("ab[0-2]");
    EXPECT_TRUE(matches(nfa, "ab0"));
    EXPECT_TRUE(matches(nfa, "ab2"));
    EXPECT_FALSE(matches(nfa, ""));
    EXPECT_FALSE(matches(nfa, "ab"));
    EXPECT_FALSE(matches(nfa, "ab3"));
    EXPECT_FALSE(matches(nfa, "zab0"));
    EXPECT_FALSE(matches(nfa, "ab0z"));
}

TEST(RegexNfaMatcherTest, MatchesAlternationAndAllQuantifiers) {
    const auto nfa = build("a(b|c)*d+e?");
    EXPECT_TRUE(matches(nfa, "ad"));
    EXPECT_TRUE(matches(nfa, "abcbcd"));
    EXPECT_TRUE(matches(nfa, "acddde"));
    EXPECT_FALSE(matches(nfa, "a"));
    EXPECT_FALSE(matches(nfa, "abce"));
    EXPECT_FALSE(matches(nfa, "abdex"));
}

TEST(RegexNfaMatcherTest, HandlesEpsilonAndTheEmptyLanguageDifferently) {
    const auto epsilon = build("");
    EXPECT_TRUE(matches(epsilon, ""));
    EXPECT_FALSE(matches(epsilon, "a"));

    const auto optional = build("a?");
    EXPECT_TRUE(matches(optional, ""));
    EXPECT_TRUE(matches(optional, "a"));
    EXPECT_FALSE(matches(optional, "aa"));

    const auto emptyLanguage = build("[^\\x00-\\xFF]");
    EXPECT_FALSE(matches(emptyLanguage, ""));
    EXPECT_FALSE(matches(emptyLanguage, "a"));
}

TEST(RegexNfaMatcherTest, AcceptsAllByteValuesWithoutCharSignednessLeaks) {
    const auto nfa = build("\\x00[\\x80-\\xFF]");
    const std::array<std::uint8_t, 2> accepted{0x00, 0xFF};
    const std::array<std::uint8_t, 2> rejected{0x00, 0x7F};
    EXPECT_TRUE(zbik::RegexNfaMatcher::matches(nfa, accepted));
    EXPECT_FALSE(zbik::RegexNfaMatcher::matches(nfa, rejected));

    const std::string withNull{"\0\x80", 2};
    EXPECT_TRUE(matches(nfa, withNull));
}

TEST(RegexNfaMatcherTest, MatchesQuotedWhitespaceAsOrdinaryBytes) {
    const auto nfa = build("'a b'\\t");
    EXPECT_TRUE(matches(nfa, "a b\t"));
    EXPECT_FALSE(matches(nfa, "ab\t"));
    EXPECT_FALSE(matches(nfa, "a b"));
}

auto codePoints(std::string_view input) -> std::vector<zbik::CodePoint> {
    return {input.begin(), input.end()};
}

TEST(RegexNfaMatcherTest, ChoosesLazyRepetitionsFromLeftToRight) {
    const zbik::RegexParser parser;
    const zbik::RegexAst expression = zbik::RegexAst::concatenate({
            zbik::RegexAst::repeat(parser.parse("a"), zbik::RegexQuantifier::LazyZeroOrMore),
            parser.parse("a"),
            zbik::RegexAst::repeat(parser.parse("b"), zbik::RegexQuantifier::LazyZeroOrMore),
            parser.parse("b"),
    });
    const zbik::RegexNfa nfa = zbik::RegexNfa::fromRegex(expression);

    const auto input = codePoints("aaabbb");
    EXPECT_EQ(zbik::RegexNfaMatcher::preferredPrefixLength(nfa, input), 4U);
}

TEST(RegexNfaMatcherTest, PrefersTheFirstAlternativeAfterLazyDecision) {
    const zbik::RegexParser parser;
    const zbik::RegexAst expression = zbik::RegexAst::concatenate({
            zbik::RegexAst::repeat(zbik::RegexAst::codePointClass(zbik::CodePointClass({
                                           {0, zbik::maxUnicodeCodePoint},
                                   })),
                                   zbik::RegexQuantifier::LazyZeroOrMore),
            zbik::RegexAst::alternate({parser.parse("a"), parser.parse("ab")}),
    });
    const zbik::RegexNfa nfa = zbik::RegexNfa::fromRegex(expression);

    const auto input = codePoints("zzab");
    EXPECT_EQ(zbik::RegexNfaMatcher::preferredPrefixLength(nfa, input), 3U);
}

void enumerateBinaryWords(std::size_t remaining, std::string &word, const zbik::RegexNfa &nfa) {
    const bool expected = word.size() >= 3 && word.ends_with("abb");
    EXPECT_EQ(matches(nfa, word), expected) << "word=" << word;
    if (remaining == 0)
        return;
    for (const char byte: {'a', 'b'}) {
        word.push_back(byte);
        enumerateBinaryWords(remaining - 1, word, nfa);
        word.pop_back();
    }
}

TEST(RegexNfaMatcherTest, ExhaustivelyChecksAClassicThompsonExample) {
    const auto nfa = build("(a|b)*abb");
    std::string word;
    enumerateBinaryWords(6, word, nfa);
}

} // namespace
