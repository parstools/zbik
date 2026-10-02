#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "first/Nullable.h"
#include "grammar/GrammarAnalysis.h"
#include "grammar/GrammarBuilder.h"
#include "grammar/YieldLength.h"

namespace {

zbik::NonterminalId nt(const zbik::Grammar &grammar, const std::string &name) {
    return grammar.findNonterminal(name).value();
}

std::vector<std::string> hugeFiniteGrammar() {
    constexpr std::size_t bits = std::numeric_limits<std::size_t>::digits;
    std::vector<std::string> rules{
            "S -> Exact", "Over -> Exact t", "Mixed -> Over Dead",
            "Dead -> Dead",
    };
    std::string exact = "Exact ->";
    for (std::size_t i = bits; i > 0; --i) {
        exact += " P" + std::to_string(i - 1);
    }
    rules.insert(rules.begin() + 1, std::move(exact));
    rules.push_back("P0 -> t");
    for (std::size_t i = 1; i < bits; ++i) {
        rules.push_back("P" + std::to_string(i) + " -> P"
                        + std::to_string(i - 1) + " P" + std::to_string(i - 1));
    }
    return rules;
}

} // namespace

TEST(MinYieldLengthTest, ComputesSymbolsRulesAndRepeatedOccurrences) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A A", "A ->", "A -> token", "A -> Dead",
            "Dead -> Dead", "Unused -> value",
    });
    const zbik::MinYieldLength lengths(grammar);

    EXPECT_EQ(lengths.minimum(grammar.start()), zbik::MinYield::finite(0));
    EXPECT_EQ(lengths.minimum(zbik::RuleId{0}), zbik::MinYield::finite(0));
    EXPECT_EQ(lengths.minimum(zbik::RuleId{2}), zbik::MinYield::finite(1));
    EXPECT_EQ(lengths.minimum(grammar.findTerminal("token").value()),
              zbik::MinYield::finite(1));
    EXPECT_EQ(lengths.minimum(nt(grammar, "Unused")), zbik::MinYield::finite(1));
    EXPECT_EQ(lengths.minimum(nt(grammar, "Dead")), zbik::MinYield::noYield());
    EXPECT_EQ(lengths.minimum(zbik::RuleId{3}), zbik::MinYield::noYield());
}

TEST(MinYieldLengthTest, HandlesRecursiveFixedPointsAndRuleOrder) {
    const auto first = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> B", "B -> S", "B -> x",
    });
    const auto second = zbik::GrammarBuilder{}.build({
            "S -> A", "B -> x", "B -> S", "A -> B",
    });
    EXPECT_EQ(zbik::MinYieldLength(first).minimum(first.start()),
              zbik::MinYield::finite(1));
    EXPECT_EQ(zbik::MinYieldLength(second).minimum(second.start()),
              zbik::MinYield::finite(1));

    const auto nullable = zbik::GrammarBuilder{}.build({"S -> A", "A -> S", "A ->"});
    const auto empty = zbik::GrammarBuilder{}.build({"S -> A", "A -> S"});
    EXPECT_EQ(zbik::MinYieldLength(nullable).minimum(nullable.start()),
              zbik::MinYield::finite(0));
    EXPECT_EQ(zbik::MinYieldLength(empty).minimum(empty.start()),
              zbik::MinYield::noYield());
}

TEST(MinYieldLengthTest, ReportsDiagnosticsAndAgreesWithExistingAnalyses) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "A ->", "A -> Dead", "Dead -> Dead", "Unused -> u",
    });
    const zbik::MinYieldLength lengths(grammar);
    const zbik::GrammarAnalysis analysis(grammar);
    const zbik::NullableAnalysis nullable(grammar);

    EXPECT_FALSE(lengths.checkMinLen());
    EXPECT_TRUE(std::ranges::equal(lengths.noYieldNonterminals(),
                                   std::vector{nt(grammar, "Dead")}));
    EXPECT_TRUE(std::ranges::equal(lengths.noYieldRules(),
                                   std::vector{zbik::RuleId{2}, zbik::RuleId{3}}));
    for (std::size_t i = 0; i < grammar.nonterminalCount(); ++i) {
        const zbik::NonterminalId id{static_cast<std::uint32_t>(i)};
        EXPECT_EQ(lengths.minimum(id).hasYield(), analysis.isProductive(id));
        EXPECT_EQ(lengths.minimum(id) == zbik::MinYield::finite(0),
                  nullable.isNullable(id));
    }
    for (const auto &rule: grammar.rules()) {
        EXPECT_EQ(lengths.minimum(rule.id()).hasYield(),
                  analysis.isProductive(rule.id()));
        EXPECT_EQ(lengths.minimum(rule.id()) == zbik::MinYield::finite(0),
                  nullable.isNullable(rule.rhs()));
    }
}

TEST(MinYieldLengthTest, SeparatesExactSizeTOverflowAndNoYield) {
    const auto grammar = zbik::GrammarBuilder{}.build(hugeFiniteGrammar());
    const zbik::MinYieldLength lengths(grammar);

    EXPECT_EQ(lengths.minimum(nt(grammar, "Exact")),
              zbik::MinYield::finite(std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(lengths.minimum(nt(grammar, "Over")),
              zbik::MinYield::exceedsSizeT());
    EXPECT_EQ(lengths.minimum(nt(grammar, "Mixed")), zbik::MinYield::noYield());
    EXPECT_EQ(lengths.minimum(nt(grammar, "Dead")), zbik::MinYield::noYield());
    EXPECT_TRUE(std::ranges::equal(lengths.exceededNonterminals(),
                                   std::vector{nt(grammar, "Over")}));
    EXPECT_THROW(static_cast<void>(lengths.minimum(nt(grammar, "Over")).value()),
                 std::logic_error);
    EXPECT_THROW(static_cast<void>(lengths.minimum(nt(grammar, "Dead")).value()),
                 std::logic_error);
}

TEST(MinYieldLengthTest, ShortAlternativeWinsRegardlessOfRuleOrder) {
    auto rules = hugeFiniteGrammar();
    rules.insert(rules.begin() + 1, "S -> short");
    const auto first = zbik::GrammarBuilder{}.build(rules);
    EXPECT_EQ(zbik::MinYieldLength(first).minimum(first.start()),
              zbik::MinYield::finite(1));

    rules.erase(rules.begin() + 1);
    rules.push_back("S -> short");
    const auto last = zbik::GrammarBuilder{}.build(rules);
    EXPECT_EQ(zbik::MinYieldLength(last).minimum(last.start()),
              zbik::MinYield::finite(1));
}

TEST(MaxYieldLengthTest, DistinguishesZeroGrowthUnboundedGrowthAndNoYield) {
    const auto fixed = zbik::GrammarBuilder{}.build({"S -> S", "S -> a"});
    const auto emptyFixed = zbik::GrammarBuilder{}.build({"S -> S", "S ->"});
    const auto growing = zbik::GrammarBuilder{}.build({"S -> S a", "S -> a"});
    const auto noYield = zbik::GrammarBuilder{}.build({"S -> S"});

    EXPECT_EQ(zbik::MaxYieldLength(fixed).maximum(fixed.start()),
              zbik::MaxYield::finite(1));
    EXPECT_EQ(zbik::MaxYieldLength(emptyFixed).maximum(emptyFixed.start()),
              zbik::MaxYield::finite(0));
    EXPECT_EQ(zbik::MaxYieldLength(growing).maximum(growing.start()),
              zbik::MaxYield::unbounded());
    EXPECT_EQ(zbik::MaxYieldLength(noYield).maximum(noYield.start()),
              zbik::MaxYield::noYield());
}

TEST(MaxYieldLengthTest, DetectsBranchingGrowthWithANullableContext) {
    const auto growing = zbik::GrammarBuilder{}.build({
            "S -> S S", "S ->", "S -> a",
    });
    const auto zeroOnly = zbik::GrammarBuilder{}.build({"S -> S S", "S ->"});

    EXPECT_EQ(zbik::MaxYieldLength(growing).maximum(growing.start()),
              zbik::MaxYield::unbounded());
    EXPECT_EQ(zbik::MaxYieldLength(zeroOnly).maximum(zeroOnly.start()),
              zbik::MaxYield::finite(0));
}

TEST(MaxYieldLengthTest, PropagatesUnboundednessButKeepsFiniteAlternatives) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "S -> one", "A -> A more", "A -> base",
    });
    const zbik::MaxYieldLength lengths(grammar);

    EXPECT_EQ(lengths.maximum(grammar.start()), zbik::MaxYield::unbounded());
    EXPECT_EQ(lengths.maximum(nt(grammar, "A")), zbik::MaxYield::unbounded());
    EXPECT_EQ(lengths.maximum(zbik::RuleId{0}), zbik::MaxYield::unbounded());
    EXPECT_EQ(lengths.maximum(zbik::RuleId{1}), zbik::MaxYield::finite(1));
    EXPECT_EQ(lengths.maximum(zbik::RuleId{3}), zbik::MaxYield::finite(1));
}

TEST(MaxYieldLengthTest, HandlesMutualZeroGrowthRecursion) {
    const auto grammar = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> B", "B -> S", "B -> token",
    });
    const zbik::MaxYieldLength lengths(grammar);
    EXPECT_EQ(lengths.maximum(grammar.start()), zbik::MaxYield::finite(1));
    EXPECT_EQ(lengths.maximum(nt(grammar, "A")), zbik::MaxYield::finite(1));
    EXPECT_EQ(lengths.maximum(nt(grammar, "B")), zbik::MaxYield::finite(1));
}

TEST(MaxYieldLengthTest, RequiresProductivePositiveGrowthInsideTheCycle) {
    const auto growing = zbik::GrammarBuilder{}.build({
            "S -> A", "A -> B", "A -> base", "B -> A C", "C ->", "C -> more",
    });
    const auto deadContext = zbik::GrammarBuilder{}.build({
            "S -> S Dead", "S -> base", "Dead -> Dead",
    });

    EXPECT_EQ(zbik::MaxYieldLength(growing).maximum(growing.start()),
              zbik::MaxYield::unbounded());
    EXPECT_EQ(zbik::MaxYieldLength(deadContext).maximum(deadContext.start()),
              zbik::MaxYield::finite(1));
}

TEST(MaxYieldLengthTest, KeepsFiniteOverflowSeparateFromUnboundedness) {
    const auto grammar = zbik::GrammarBuilder{}.build(hugeFiniteGrammar());
    const zbik::MaxYieldLength lengths(grammar);

    EXPECT_EQ(lengths.maximum(nt(grammar, "Exact")),
              zbik::MaxYield::finite(std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(lengths.maximum(nt(grammar, "Over")),
              zbik::MaxYield::exceedsSizeT());
    EXPECT_FALSE(lengths.maximum(nt(grammar, "Over")).isUnbounded());
    EXPECT_EQ(lengths.maximum(nt(grammar, "Mixed")), zbik::MaxYield::noYield());
}
