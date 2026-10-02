#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <unordered_set>

#include "regex/RegexDfa.h"
#include "regex/RegexParser.h"

static_assert(!std::is_same_v<zbik::DfaStateId, zbik::NfaStateId>);

namespace {

zbik::RegexNfa buildNfa(std::string_view pattern) {
    return zbik::RegexNfa::fromRegex(zbik::RegexParser{}.parse(pattern));
}

TEST(RegexDfaTest, BuildsAClassTransitionAndAnAcceptingState) {
    const auto nfa = buildNfa("[a-c]");
    const zbik::RegexDfa dfa(nfa);
    ASSERT_EQ(dfa.states().size(), 2U);
    EXPECT_EQ(dfa.startState(), (zbik::DfaStateId{0}));
    EXPECT_FALSE(dfa.state({0}).accepting);
    EXPECT_TRUE(dfa.state({1}).accepting);
    ASSERT_EQ(dfa.state({0}).byteTransitions.size(), 1U);
    const auto &transition = dfa.state({0}).byteTransitions.front();
    EXPECT_EQ(transition.target, (zbik::DfaStateId{1}));
    EXPECT_EQ(transition.bytes.ranges(),
              std::vector<zbik::ByteRange>({{'a', 'c'}}));
}

TEST(RegexDfaTest, KeepsCompleteCanonicalNfaSubsets) {
    const auto nfa = buildNfa("(a|b)*abb");
    const zbik::RegexDfa dfa(nfa);
    // Subset construction is intentionally not minimization; equivalent
    // accepting subsets remain separate until point 6.5.
    ASSERT_EQ(dfa.states().size(), 5U);
    for (const zbik::DfaState &state: dfa.states()) {
        EXPECT_TRUE(std::ranges::is_sorted(state.nfaStates));
        EXPECT_EQ(std::unordered_set<zbik::NfaStateId>(
                          state.nfaStates.begin(), state.nfaStates.end()).size(),
                  state.nfaStates.size());
    }
    EXPECT_TRUE(std::ranges::any_of(dfa.states(), [](const zbik::DfaState &state) {
        return state.accepting;
    }));
}

TEST(RegexDfaTest, CoalescesDisjointByteRangesWithTheSameTarget) {
    const auto nfa = buildNfa("[a-cx-z]");
    const zbik::RegexDfa dfa(nfa);
    ASSERT_EQ(dfa.state({0}).byteTransitions.size(), 1U);
    EXPECT_EQ(dfa.state({0}).byteTransitions.front().bytes.ranges(),
              (std::vector<zbik::ByteRange>{{'a', 'c'}, {'x', 'z'}}));
}

TEST(RegexDfaTest, DeterminizesUnicodeIntervalsWithoutExpandingCodePoints) {
    const auto nfa = buildNfa("[a-cą-ć]|[b-dć-ę]");
    const zbik::RegexDfa dfa(nfa);
    ASSERT_EQ(dfa.states().size(), 4U);
    const auto &transitions = dfa.state(dfa.startState()).byteTransitions;
    EXPECT_LE(transitions.size(), 3U);
    std::size_t rangeCount = 0;
    for (const zbik::DfaByteTransition &transition: transitions) {
        rangeCount += transition.bytes.ranges().size();
    }
    EXPECT_LE(rangeCount, 6U);
}

TEST(RegexDfaTest, ProducesDisjointTransitionsWithKnownTargets) {
    const auto nfa = buildNfa("[a-c]|[b-d]");
    const zbik::RegexDfa dfa(nfa);
    for (const zbik::DfaState &state: dfa.states()) {
        std::array<bool, 256> covered{};
        for (const zbik::DfaByteTransition &transition: state.byteTransitions) {
            EXPECT_LT(zbik::toIndex(transition.target), dfa.states().size());
            for (const zbik::ByteRange range: transition.bytes.ranges()) {
                for (unsigned byte = range.first; byte <= range.last; ++byte) {
                    EXPECT_FALSE(covered[byte]);
                    covered[byte] = true;
                }
            }
        }
    }
}

TEST(RegexDfaTest, RepresentsMissingTransitionsWithoutAMaterializedDeadState) {
    const auto nfa = buildNfa("[^\\x00-\\U0010FFFF]");
    const zbik::RegexDfa dfa(nfa);
    ASSERT_EQ(dfa.states().size(), 1U);
    EXPECT_FALSE(dfa.state({0}).accepting);
    EXPECT_TRUE(dfa.state({0}).byteTransitions.empty());
}

TEST(RegexDfaTest, MarksTheStartStateAcceptingForNullableRegexes) {
    const auto nfa = buildNfa("a*");
    const zbik::RegexDfa dfa(nfa);
    EXPECT_TRUE(dfa.state(dfa.startState()).accepting);
}

TEST(RegexDfaTest, ResolvesForcedHashCollisionsByStructuralEquality) {
    const auto nfa = buildNfa("(a|b)*abb");
    const zbik::RegexDfa regular(nfa);
    const zbik::RegexDfa colliding(
            nfa, [](const zbik::NfaStateSubset &) { return 0U; });
    EXPECT_TRUE(std::ranges::equal(colliding.states(), regular.states()));
    EXPECT_GT(colliding.states().size(), 1U);
}

TEST(RegexDfaTest, OwnsEverythingNeededAfterTheNfaIsDestroyed) {
    const auto makeDfa = [] {
        const auto nfa = buildNfa("a|b");
        return zbik::RegexDfa(nfa);
    };
    const zbik::RegexDfa dfa = makeDfa();
    EXPECT_EQ(dfa.states().size(), 3U);
    EXPECT_EQ(dfa.state({0}).byteTransitions.size(), 2U);
}

TEST(RegexDfaTest, RejectsAnEmptyHasherAndUnknownState) {
    const auto nfa = buildNfa("a");
    EXPECT_THROW((zbik::RegexDfa(nfa, {})), std::invalid_argument);
    const zbik::RegexDfa dfa(nfa);
    EXPECT_THROW((void) dfa.state({2}), std::out_of_range);
}

} // namespace
