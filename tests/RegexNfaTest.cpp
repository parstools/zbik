#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

#include "lr/Identifiers.h"
#include "regex/RegexNfa.h"
#include "regex/RegexParser.h"

static_assert(!std::is_same_v<zbik::NfaStateId, zbik::StateId>);

namespace {

zbik::RegexNfa build(std::string_view pattern) {
    return zbik::RegexNfa::fromRegex(zbik::RegexParser{}.parse(pattern));
}

TEST(RegexNfaTest, BuildsAByteClassFragmentWithDenseTypedStates) {
    const auto nfa = build("[a-c]");
    ASSERT_EQ(nfa.states().size(), 2U);
    EXPECT_EQ(nfa.startState(), (zbik::NfaStateId{0}));
    EXPECT_EQ(nfa.acceptingState(), (zbik::NfaStateId{1}));
    ASSERT_EQ(nfa.state({0}).byteTransitions.size(), 1U);
    const auto &transition = nfa.state({0}).byteTransitions.front();
    EXPECT_EQ(transition.target, (zbik::NfaStateId{1}));
    EXPECT_TRUE(transition.bytes.contains('a'));
    EXPECT_TRUE(transition.bytes.contains('c'));
    EXPECT_FALSE(transition.bytes.contains('d'));
}

TEST(RegexNfaTest, ConnectsConcatenatedFragmentsWithEpsilon) {
    const auto nfa = build("ab");
    ASSERT_EQ(nfa.states().size(), 4U);
    ASSERT_EQ(nfa.state({0}).byteTransitions.size(), 1U);
    EXPECT_EQ(nfa.state({0}).byteTransitions.front().target, (zbik::NfaStateId{1}));
    EXPECT_EQ(nfa.state({1}).epsilonTransitions, std::vector<zbik::NfaStateId>({{2}}));
    ASSERT_EQ(nfa.state({2}).byteTransitions.size(), 1U);
    EXPECT_EQ(nfa.state({2}).byteTransitions.front().target, nfa.acceptingState());
}

TEST(RegexNfaTest, BuildsAlternationWithACommonEntryAndExit) {
    const auto nfa = build("a|b");
    ASSERT_EQ(nfa.states().size(), 6U);
    EXPECT_EQ(nfa.startState(), (zbik::NfaStateId{0}));
    EXPECT_EQ(nfa.acceptingState(), (zbik::NfaStateId{5}));
    EXPECT_EQ(nfa.state({0}).epsilonTransitions, std::vector<zbik::NfaStateId>({{1}, {3}}));
    EXPECT_EQ(nfa.state({2}).epsilonTransitions, std::vector<zbik::NfaStateId>({{5}}));
    EXPECT_EQ(nfa.state({4}).epsilonTransitions, std::vector<zbik::NfaStateId>({{5}}));
}

TEST(RegexNfaTest, BuildsAllThreeQuantifiers) {
    const auto star = build("a*");
    EXPECT_EQ(star.epsilonClosure({star.startState()}), std::vector<zbik::NfaStateId>({{0}, {1}, {3}}));
    EXPECT_EQ(star.state({2}).epsilonTransitions, std::vector<zbik::NfaStateId>({{1}, {3}}));

    const auto plus = build("a+");
    EXPECT_EQ(plus.epsilonClosure({plus.startState()}), std::vector<zbik::NfaStateId>({{0}, {1}}));
    EXPECT_EQ(plus.state({2}).epsilonTransitions, std::vector<zbik::NfaStateId>({{1}, {3}}));

    const auto optional = build("a?");
    EXPECT_EQ(optional.epsilonClosure({optional.startState()}), std::vector<zbik::NfaStateId>({{0}, {1}, {3}}));
    EXPECT_EQ(optional.state({2}).epsilonTransitions, std::vector<zbik::NfaStateId>({{3}}));
}

TEST(RegexNfaTest, EpsilonClosureIsTransitiveSortedAndDuplicateFree) {
    const auto nfa = build("(a?|b*)*");
    const auto closure = nfa.epsilonClosure({nfa.startState(), nfa.startState()});
    EXPECT_TRUE(std::ranges::is_sorted(closure));
    EXPECT_EQ(std::unordered_set<zbik::NfaStateId>(closure.begin(), closure.end()).size(), closure.size());
    EXPECT_TRUE(std::ranges::find(closure, nfa.acceptingState()) != closure.end());
}

TEST(RegexNfaTest, DistinguishesEpsilonFromTheEmptyUnicodeLanguage) {
    const auto epsilon = build("");
    EXPECT_EQ(epsilon.epsilonClosure({epsilon.startState()}), std::vector<zbik::NfaStateId>({{0}, {1}}));

    const auto emptyLanguage = build("[^\\x00-\\U0010FFFF]");
    EXPECT_EQ(emptyLanguage.epsilonClosure({emptyLanguage.startState()}), std::vector<zbik::NfaStateId>({{0}}));
    ASSERT_EQ(emptyLanguage.state({0}).byteTransitions.size(), 1U);
    EXPECT_TRUE(emptyLanguage.state({0}).byteTransitions.front().bytes.ranges().empty());
}

TEST(RegexNfaTest, RejectsInvalidGraphReferencesAndClosureSeeds) {
    EXPECT_THROW((zbik::RegexNfa({{}}, {1}, {0})), std::invalid_argument);
    EXPECT_THROW((zbik::RegexNfa({{{{1}}, {}}}, {0}, {0})), std::invalid_argument);

    const auto nfa = build("a");
    EXPECT_THROW((void) nfa.state({2}), std::out_of_range);
    EXPECT_THROW((void) nfa.epsilonClosure({zbik::NfaStateId{2}}), std::out_of_range);
}

} // namespace
