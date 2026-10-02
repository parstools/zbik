#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "regex/RegexDfaMatcher.h"
#include "regex/RegexNfaMatcher.h"
#include "regex/RegexParser.h"

namespace {

struct Automata {
    zbik::RegexNfa nfa;
    zbik::RegexDfa dfa;
    zbik::MinimizedRegexDfa minimized;

    explicit Automata(std::string_view pattern)
        : nfa(zbik::RegexNfa::fromRegex(zbik::RegexParser{}.parse(pattern))),
          dfa(nfa),
          minimized(zbik::RegexDfaMinimizer::minimize(dfa)) {}
};

void expectSameDecision(const Automata &automata, std::string_view input) {
    const bool expected = zbik::RegexNfaMatcher::matches(automata.nfa, input);
    EXPECT_EQ(zbik::RegexDfaMatcher::matches(automata.dfa, input), expected)
            << "input=" << input;
    EXPECT_EQ(zbik::RegexDfaMatcher::matches(automata.minimized, input), expected)
            << "input=" << input;
}

void enumerate(
        const Automata &automata,
        std::string_view alphabet,
        std::size_t remaining,
        std::string &word) {
    expectSameDecision(automata, word);
    if (remaining == 0) return;
    for (const char byte: alphabet) {
        word.push_back(byte);
        enumerate(automata, alphabet, remaining - 1, word);
        word.pop_back();
    }
}

TEST(RegexDfaMatcherTest, MatchesWithTheUnminimizedDfa) {
    const Automata automata("a(b|c)*d+e?");
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(automata.dfa, "ad"));
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(automata.dfa, "abcbcdde"));
    EXPECT_FALSE(zbik::RegexDfaMatcher::matches(automata.dfa, "a"));
    EXPECT_FALSE(zbik::RegexDfaMatcher::matches(automata.dfa, "abce"));
}

TEST(RegexDfaMatcherTest, MinimizesAClassicSuffixLanguage) {
    const Automata automata("(a|b)*abb");
    EXPECT_EQ(automata.dfa.states().size(), 5U);
    EXPECT_EQ(automata.minimized.states().size(), 4U);
    EXPECT_EQ(automata.minimized.startState(), (zbik::DfaStateId{0}));
    EXPECT_TRUE(std::ranges::any_of(
            automata.minimized.states(), [](const zbik::MinimizedDfaState &state) {
                return state.originalStates.size() > 1;
            }));
}

TEST(RegexDfaMatcherTest, KeepsAnEmptyLanguageAsOneRejectingStartState) {
    const Automata automata("[^\\x00-\\U0010FFFF]");
    ASSERT_EQ(automata.minimized.states().size(), 1U);
    EXPECT_FALSE(automata.minimized.state({0}).accepting);
    EXPECT_TRUE(automata.minimized.state({0}).byteTransitions.empty());
    expectSameDecision(automata, "");
    expectSameDecision(automata, "a");
}

TEST(RegexDfaMatcherTest, PreservesNullableLanguages) {
    const Automata epsilon("");
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(epsilon.minimized, ""));
    EXPECT_FALSE(zbik::RegexDfaMatcher::matches(epsilon.minimized, "a"));

    const Automata star("a*");
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(star.minimized, ""));
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(star.minimized, "aaaa"));
    EXPECT_FALSE(zbik::RegexDfaMatcher::matches(star.minimized, "b"));
}

TEST(RegexDfaMatcherTest, HandlesTheWholeByteAlphabet) {
    const Automata automata("\\x00[\\x80-\\xFF]");
    const std::array<std::uint8_t, 2> accepted{0x00, 0xFF};
    const std::array<std::uint8_t, 2> rejected{0x00, 0x7F};
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(automata.dfa, accepted));
    EXPECT_TRUE(zbik::RegexDfaMatcher::matches(automata.minimized, accepted));
    EXPECT_FALSE(zbik::RegexDfaMatcher::matches(automata.dfa, rejected));
    EXPECT_FALSE(zbik::RegexDfaMatcher::matches(automata.minimized, rejected));
}

TEST(RegexDfaMatcherTest, MinimizesTransitionsAcrossUnicodeIntervals) {
    const Automata automata("([a-z]|[ą-ć])+");
    ASSERT_EQ(automata.minimized.states().size(), 2U);
    const auto &start = automata.minimized.state(
            automata.minimized.startState());
    ASSERT_EQ(start.byteTransitions.size(), 1U);
    EXPECT_TRUE(start.byteTransitions.front().bytes.contains(U'a'));
    EXPECT_TRUE(start.byteTransitions.front().bytes.contains(U'ą'));
    EXPECT_FALSE(start.byteTransitions.front().bytes.contains(U'😀'));
}

TEST(RegexDfaMatcherTest, ExhaustivelyAgreesWithTheNfaOracle) {
    const std::vector<std::string_view> patterns = {
            "", "a", "a|b", "a*", "a?b+", "(a|b)*abb",
            "a(b|c)?", "[a-c]*", "[^\\x00-\\U0010FFFF]",
    };
    for (const std::string_view pattern: patterns) {
        const Automata automata(pattern);
        std::string word;
        enumerate(automata, "abc", 5, word);
    }
}

TEST(RegexDfaMatcherTest, MinimizedTransitionsRemainDisjointAndInRange) {
    const Automata automata("([a-c]|[b-d])*abb");
    for (const zbik::MinimizedDfaState &state: automata.minimized.states()) {
        std::array<bool, 256> covered{};
        for (const zbik::DfaByteTransition &transition: state.byteTransitions) {
            EXPECT_LT(zbik::toIndex(transition.target),
                      automata.minimized.states().size());
            for (const zbik::ByteRange range: transition.bytes.ranges()) {
                for (unsigned byte = range.first; byte <= range.last; ++byte) {
                    EXPECT_FALSE(covered[byte]);
                    covered[byte] = true;
                }
            }
        }
    }
}

TEST(RegexDfaMatcherTest, RejectsUnknownMinimizedState) {
    const Automata automata("a");
    EXPECT_THROW((void) automata.minimized.state({2}), std::out_of_range);
}

} // namespace
