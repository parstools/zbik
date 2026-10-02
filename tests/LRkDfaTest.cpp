#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "grammar/GrammarBuilder.h"
#include "lr/LRkDfa.h"
#include "lr/LRkGoto.h"
#include "lr/ParseTable.h"

namespace {

class LRkDfaTest : public testing::TestWithParam<bool> {
protected:
    zbik::LRkDfa build(const zbik::Grammar &grammar, std::size_t k) {
        return zbik::LRkDfa(grammar, k, GetParam()
                ? zbik::LRkDfa::StateHasher{[](const zbik::ItemSet &) { return 0U; }}
                : zbik::LRkDfa::StateHasher{zbik::LRkDfa::hashItems});
    }
};

} // namespace

TEST(LRkDfaContractTest, ReusesCompatiblePrecomputedFirstK) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A x y", "A -> a"});
    const zbik::FirstKAnalysis first(grammar, 2);
    const zbik::LRkDfa direct(grammar, 2);
    const zbik::LRkDfa reused(grammar, first);

    EXPECT_EQ(reused.maxLength(), 2U);
    EXPECT_TRUE(std::ranges::equal(reused.states(), direct.states()));

    const auto other = zbik::GrammarBuilder{}.build({"S -> b"});
    const zbik::FirstKAnalysis wrongFirst(other, 2);
    EXPECT_THROW(
            static_cast<void>(zbik::LRkDfa(grammar, wrongFirst)),
            std::invalid_argument);
}

TEST(LRkDfaContractTest, StopsAtAConflictWithoutPublishingAPartialDfa) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A",
            "S -> B",
            "A ->",
            "B ->",
    });
    const zbik::FirstKAnalysis first(grammar, 1);
    const zbik::LRkDfa complete(grammar, first);
    const zbik::ParseTable completeTable(complete);
    const auto probe = zbik::LRkDfa::buildUntilFirstConflict(grammar, first);

    ASSERT_TRUE(completeTable.hasConflicts());
    EXPECT_TRUE(probe.conflict);
    EXPECT_FALSE(probe.dfa.has_value());
    EXPECT_LT(probe.completedStates, complete.states().size());
    EXPECT_GE(probe.discoveredStates, probe.completedStates);
}

TEST(LRkDfaContractTest, ReturnsACompleteDfaWhenTheProbeFindsNoConflict) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::FirstKAnalysis first(grammar, 1);
    const auto probe = zbik::LRkDfa::buildUntilFirstConflict(grammar, first);

    EXPECT_FALSE(probe.conflict);
    ASSERT_TRUE(probe.dfa.has_value());
    EXPECT_EQ(probe.completedStates, probe.dfa->states().size());
    EXPECT_EQ(probe.discoveredStates, probe.dfa->states().size());
    EXPECT_FALSE(zbik::ParseTable(*probe.dfa).hasConflicts());
}

TEST_P(LRkDfaTest, BuildsTheExactThreeStateAutomaton) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const zbik::LRGrammarView view(grammar);
    const auto dfa = build(grammar, 1);
    const zbik::LookaheadWord eof{{zbik::endOfInput}};
    ASSERT_EQ(dfa.states().size(), 3U);
    EXPECT_EQ(dfa.start(), (zbik::StateId{0}));
    EXPECT_EQ(dfa.state({0}).items, (zbik::ItemSet{
            zbik::Item(view, {0}, 0, eof), zbik::Item(view, view.syntheticRuleId(), 0, eof)}));
    EXPECT_EQ(dfa.state({1}).items, (zbik::ItemSet{zbik::Item(view, {0}, 1, eof)}));
    EXPECT_EQ(dfa.state({2}).items, (zbik::ItemSet{zbik::Item(view, view.syntheticRuleId(), 1, eof)}));
    EXPECT_EQ(dfa.state({0}).transitions.size(), 2U);
    EXPECT_EQ(dfa.state({0}).transitions.at(grammar.findTerminal("a").value()), (zbik::StateId{1}));
    EXPECT_EQ(dfa.state({0}).transitions.at(grammar.start()), (zbik::StateId{2}));
    EXPECT_TRUE(dfa.state({1}).transitions.empty());
    EXPECT_TRUE(dfa.state({2}).transitions.empty());
}

TEST_P(LRkDfaTest, BuildsEpsilonGrammarWithoutAnEmptyState) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S ->"});
    const auto dfa = build(grammar, 3);
    ASSERT_EQ(dfa.states().size(), 2U);
    EXPECT_EQ(dfa.state({0}).items.size(), 2U);
    EXPECT_EQ(dfa.state({0}).transitions.size(), 1U);
    EXPECT_EQ(dfa.state({0}).transitions.at(grammar.start()), (zbik::StateId{1}));
    EXPECT_EQ(dfa.state({1}).items.size(), 1U);
    EXPECT_TRUE(dfa.state({1}).transitions.empty());
}

TEST_P(LRkDfaTest, ReusesAnExistingStateOnARecursiveLoop) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a S", "S -> b"});
    const auto dfa = build(grammar, 2);
    const zbik::SymbolRef a = grammar.findTerminal("a").value();
    const auto target = dfa.state({0}).transitions.at(a);
    EXPECT_EQ(dfa.states().size(), 5U);
    EXPECT_EQ(dfa.state(target).transitions.at(a), target);
}

TEST_P(LRkDfaTest, KeepsEqualCoresWithDifferentLookaheadsSeparate) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> a A d", "S -> b A e", "S -> a B e", "S -> b B d",
            "A -> c", "B -> c",
    });
    const auto dfa = build(grammar, 1);
    const auto afterA = dfa.state({0}).transitions.at(grammar.findTerminal("a").value());
    const auto afterB = dfa.state({0}).transitions.at(grammar.findTerminal("b").value());
    const zbik::SymbolRef c = grammar.findTerminal("c").value();
    const auto ac = dfa.state(afterA).transitions.at(c);
    const auto bc = dfa.state(afterB).transitions.at(c);
    ASSERT_NE(ac, bc);
    const auto &left = dfa.state(ac).items;
    const auto &right = dfa.state(bc).items;
    ASSERT_EQ(left.size(), 2U);
    ASSERT_EQ(right.size(), 2U);
    EXPECT_EQ(left[0].core(), right[0].core());
    EXPECT_EQ(left[1].core(), right[1].core());
    EXPECT_NE(left, right);
}

TEST_P(LRkDfaTest, PreservesExactMultiTokenLookaheads) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A x y", "A -> a"});
    const auto x = grammar.findTerminal("x").value();
    const auto y = grammar.findTerminal("y").value();
    const zbik::LRGrammarView view(grammar);
    for (const std::size_t k: {2U, 3U}) {
        const auto dfa = build(grammar, k);
        const zbik::LookaheadWord expected = k == 2
                ? zbik::LookaheadWord{{x, y}} : zbik::LookaheadWord{{x, y, zbik::endOfInput}};
        EXPECT_TRUE(std::ranges::find(dfa.state({0}).items,
                zbik::Item(view, {1}, 0, expected)) != dfa.state({0}).items.end());
        const auto target = dfa.state({0}).transitions.at(grammar.findTerminal("a").value());
        EXPECT_EQ(dfa.state(target).items, (zbik::ItemSet{zbik::Item(view, {1}, 1, expected)}));
    }
}

TEST_P(LRkDfaTest, IsDeterministicAndMatchesEveryGoto) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> C C", "C -> c C", "C -> d", "C ->"});
    for (const std::size_t k: {1U, 2U, 3U}) {
        const auto dfa = build(grammar, k);
        const zbik::LRkDfa reference(grammar, k);
        ASSERT_EQ(dfa.states().size(), reference.states().size());
        EXPECT_TRUE(std::ranges::equal(dfa.states(), reference.states()));
        const zbik::LRGrammarView view(grammar);
        const zbik::FirstKAnalysis first(grammar, k);
        const zbik::LRkClosure closure(view, first);
        const zbik::LRkGoto goTo(closure);
        std::vector<zbik::SymbolRef> alphabet;
        for (std::size_t i = 0; i < view.terminalCount(); ++i) {
            alphabet.emplace_back(zbik::TerminalId{static_cast<std::uint32_t>(i)});
        }
        for (std::size_t i = 0; i < view.nonterminalCount(); ++i) {
            alphabet.emplace_back(zbik::NonterminalId{static_cast<std::uint32_t>(i)});
        }
        for (const auto &state: dfa.states()) {
            EXPECT_FALSE(state.items.empty());
            EXPECT_EQ(closure.close(state.items), state.items);
            for (const auto &[symbol, target]: state.transitions) {
                ASSERT_LT(target.value, dfa.states().size());
                EXPECT_EQ(goTo.goTo(state.items, symbol), dfa.state(target).items);
            }
            for (const auto &symbol: alphabet) {
                const auto expected = goTo.goTo(state.items, symbol);
                const auto edge = state.transitions.find(symbol);
                if (expected.empty()) {
                    EXPECT_EQ(edge, state.transitions.end());
                } else {
                    ASSERT_NE(edge, state.transitions.end());
                    EXPECT_EQ(dfa.state(edge->second).items, expected);
                }
            }
        }

        std::vector<bool> reached(dfa.states().size(), false);
        std::vector<zbik::StateId> pending{dfa.start()};
        reached.at(dfa.start().value) = true;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            for (const auto &[symbol, target]: dfa.state(pending[i]).transitions) {
                ASSERT_LT(target.value, reached.size());
                if (!reached[target.value]) {
                    reached[target.value] = true;
                    pending.push_back(target);
                }
            }
        }
        EXPECT_EQ(pending.size(), dfa.states().size());
        for (std::size_t i = 0; i < dfa.states().size(); ++i) {
            for (std::size_t j = i + 1; j < dfa.states().size(); ++j) {
                EXPECT_NE(dfa.state({i}).items, dfa.state({j}).items);
            }
        }
    }
}

TEST_P(LRkDfaTest, OwnsTheGraphAfterTheSourceGrammarIsDestroyed) {
    const auto makeDfa = [this] {
        const auto localGrammar = zbik::GrammarBuilder{}.build({"S -> a S", "S -> b"});
        return build(localGrammar, 2);
    };
    const auto dfa = makeDfa();
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a S", "S -> b"});
    const zbik::LRkDfa reference(grammar, 2);

    EXPECT_EQ(dfa.maxLength(), 2U);
    EXPECT_EQ(dfa.start(), reference.start());
    ASSERT_EQ(dfa.states().size(), 5U);
    EXPECT_TRUE(std::ranges::equal(dfa.states(), reference.states()));
    for (const auto &state: dfa.states()) {
        for (const auto &[symbol, target]: state.transitions) {
            EXPECT_EQ(dfa.state(target), reference.state(target));
        }
    }
}

TEST_P(LRkDfaTest, RejectsZeroLookahead) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    EXPECT_THROW(static_cast<void>(build(grammar, 0)), std::invalid_argument);
}

TEST_P(LRkDfaTest, DumpsTheExactSmallAutomatonAndStatistics) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a"});
    const auto dfa = build(grammar, 1);
    EXPECT_EQ(dfa.statistics(), (zbik::LRkDfaStats{3, 4, 2}));
    EXPECT_EQ(dfa.dump(),
            "LR(1) states=3 items=4 transitions=2\n"
            "state 0 (start):\n"
            "  R0: N:\"S\" -> . T:\"a\" , [EOF]\n"
            "  R1: N:\"S′\" -> . N:\"S\" , [EOF]\n"
            "  T:\"a\" -> 1\n"
            "  N:\"S\" -> 2\n"
            "state 1:\n"
            "  R0: N:\"S\" -> T:\"a\" . , [EOF]\n"
            "state 2:\n"
            "  R1: N:\"S′\" -> N:\"S\" . , [EOF]\n");
    EXPECT_EQ(dfa.toDot(),
            "digraph LRkDfa {\n  rankdir=LR;\n  node [shape=box];\n"
            "  start [shape=point,label=\"\"];\n  start -> s0;\n"
            "  s0 [label=\"state 0\\nR0: N:\\\"S\\\" -> . T:\\\"a\\\" , [EOF]"
            "\\nR1: N:\\\"S′\\\" -> . N:\\\"S\\\" , [EOF]\"];\n"
            "  s0 -> s1 [label=\"T:\\\"a\\\"\"];\n"
            "  s0 -> s2 [label=\"N:\\\"S\\\"\"];\n"
            "  s1 [label=\"state 1\\nR0: N:\\\"S\\\" -> T:\\\"a\\\" . , [EOF]\"];\n"
            "  s2 [label=\"state 2\\nR1: N:\\\"S′\\\" -> N:\\\"S\\\" . , [EOF]\"];\n}\n");
}

TEST_P(LRkDfaTest, DiagnosticsSurviveGrammarDestructionAndHashCollisions) {
    const auto dfa = [this] {
        const auto grammar = zbik::GrammarBuilder{}.build({"S -> A EOF", "A ->", "A -> a"});
        return build(grammar, 2);
    }();
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> A EOF", "A ->", "A -> a"});
    const zbik::LRkDfa reference(grammar, 2);
    EXPECT_EQ(dfa.dump(), reference.dump());
    EXPECT_EQ(dfa.toDot(), reference.toDot());
    EXPECT_EQ(dfa.statistics(), reference.statistics());
    EXPECT_NE(dfa.dump().find("R1: N:\"A\" -> . , [T:\"EOF\" EOF]"), std::string::npos);
}

TEST_P(LRkDfaTest, EscapesQuotesAndBackslashesInDotLabels) {
    const auto grammar = zbik::GrammarBuilder{}.build({"S -> a\"b\\c"});
    const auto dfa = build(grammar, 1);
    EXPECT_NE(dfa.dump().find(R"(T:"a\"b\\c")"), std::string::npos);
    EXPECT_NE(dfa.toDot().find(R"(label="T:\"a\\\"b\\\\c\"")"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(HashModes, LRkDfaTest, testing::Bool());
