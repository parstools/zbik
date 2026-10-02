#pragma once

#include <compare>
#include <cstddef>
#include <span>
#include <vector>

#include "Grammar.h"
#include "first/Nullable.h"

namespace zbik {

struct GrammarDependencyEdge {
    NonterminalId from;
    NonterminalId to;
    RuleId rule;
    std::size_t rhsPosition;

    auto operator<=>(const GrammarDependencyEdge &) const = default;
};

struct GrammarCycleComponent {
    std::vector<NonterminalId> nonterminals;
    std::vector<GrammarDependencyEdge> witness;

    bool operator==(const GrammarCycleComponent &) const = default;
};

// Computes immutable structural properties used by diagnostics and generators.
class GrammarAnalysis {
public:
    explicit GrammarAnalysis(const Grammar &grammar);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] bool isProductive(NonterminalId nonterminal) const;
    [[nodiscard]] bool isProductive(RuleId rule) const;
    [[nodiscard]] bool isReachable(NonterminalId nonterminal) const;
    [[nodiscard]] bool isReachable(RuleId rule) const;
    [[nodiscard]] bool isUseful(NonterminalId nonterminal) const;
    [[nodiscard]] bool isUseful(RuleId rule) const;
    [[nodiscard]] std::size_t nonNullableCount(RuleId rule) const;

    [[nodiscard]] std::span<const GrammarDependencyEdge> zeroProgressEdges() const noexcept;
    [[nodiscard]] std::span<const GrammarCycleComponent> zeroProgressCycles() const noexcept;
    [[nodiscard]] std::span<const GrammarDependencyEdge> leftCornerEdges() const noexcept;
    [[nodiscard]] std::span<const GrammarCycleComponent> leftRecursiveCycles() const noexcept;

private:
    const Grammar &grammar_;
    NullableAnalysis nullable_;
    std::vector<bool> productiveNonterminals_;
    std::vector<bool> productiveRules_;
    std::vector<bool> reachableNonterminals_;
    std::vector<bool> reachableRules_;
    std::vector<bool> usefulNonterminals_;
    std::vector<bool> usefulRules_;
    std::vector<std::size_t> nonNullableCounts_;
    std::vector<GrammarDependencyEdge> zeroProgressEdges_;
    std::vector<GrammarCycleComponent> zeroProgressCycles_;
    std::vector<GrammarDependencyEdge> leftCornerEdges_;
    std::vector<GrammarCycleComponent> leftRecursiveCycles_;
};

} // namespace zbik
