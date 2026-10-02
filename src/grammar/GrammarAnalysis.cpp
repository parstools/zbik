#include "GrammarAnalysis.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace zbik {
namespace {

bool productiveSymbol(const SymbolRef &symbol, const std::vector<bool> &productive) {
    if (std::holds_alternative<TerminalId>(symbol)) {
        return true;
    }
    return productive.at(toIndex(std::get<NonterminalId>(symbol)));
}

std::vector<std::vector<std::size_t>> adjacency(
        std::size_t nodeCount, std::span<const GrammarDependencyEdge> edges, bool reverse) {
    std::vector<std::vector<std::size_t>> result(nodeCount);
    for (const auto &edge: edges) {
        const std::size_t from = toIndex(reverse ? edge.to : edge.from);
        const std::size_t to = toIndex(reverse ? edge.from : edge.to);
        result.at(from).push_back(to);
    }
    for (auto &targets: result) {
        std::ranges::sort(targets);
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    }
    return result;
}

std::vector<GrammarDependencyEdge> cycleWitness(
        const std::vector<NonterminalId> &component,
        std::span<const GrammarDependencyEdge> edges) {
    std::vector<bool> inside;
    std::size_t nodeCount = 0;
    for (const auto node: component) {
        nodeCount = std::max(nodeCount, toIndex(node) + 1);
    }
    inside.resize(nodeCount, false);
    for (const auto node: component) {
        inside[toIndex(node)] = true;
    }

    std::vector<GrammarDependencyEdge> internal;
    for (const auto &edge: edges) {
        if (toIndex(edge.from) < inside.size() && toIndex(edge.to) < inside.size()
                && inside[toIndex(edge.from)] && inside[toIndex(edge.to)]) {
            internal.push_back(edge);
        }
    }
    std::ranges::sort(internal);
    if (internal.empty()) {
        throw std::logic_error("cyclic component has no internal edge");
    }

    const GrammarDependencyEdge first = internal.front();
    if (first.from == first.to) {
        return {first};
    }

    std::vector<std::vector<GrammarDependencyEdge>> outgoing(inside.size());
    for (const auto &edge: internal) {
        outgoing[toIndex(edge.from)].push_back(edge);
    }

    const std::size_t start = toIndex(first.to);
    const std::size_t target = toIndex(first.from);
    std::vector<bool> seen(inside.size(), false);
    std::vector<std::optional<GrammarDependencyEdge>> predecessor(inside.size());
    std::queue<std::size_t> pending;
    seen[start] = true;
    pending.push(start);
    while (!pending.empty() && !seen[target]) {
        const std::size_t from = pending.front();
        pending.pop();
        for (const auto &edge: outgoing[from]) {
            const std::size_t to = toIndex(edge.to);
            if (!seen[to]) {
                seen[to] = true;
                predecessor[to] = edge;
                pending.push(to);
            }
        }
    }
    if (!seen[target]) {
        throw std::logic_error("strongly connected component has no return path");
    }

    std::vector<GrammarDependencyEdge> returnPath;
    for (std::size_t current = target; current != start;) {
        const auto &edge = predecessor[current];
        if (!edge) {
            throw std::logic_error("incomplete cycle predecessor chain");
        }
        returnPath.push_back(*edge);
        current = toIndex(edge->from);
    }
    std::ranges::reverse(returnPath);

    std::vector<GrammarDependencyEdge> result{first};
    result.insert(result.end(), returnPath.begin(), returnPath.end());
    return result;
}

std::vector<GrammarCycleComponent> cyclicComponents(
        std::size_t nodeCount, std::span<const GrammarDependencyEdge> edges) {
    const auto forward = adjacency(nodeCount, edges, false);
    const auto reverse = adjacency(nodeCount, edges, true);
    std::vector<bool> visited(nodeCount, false);
    std::vector<std::size_t> finishOrder;

    for (std::size_t root = 0; root < nodeCount; ++root) {
        if (visited[root]) {
            continue;
        }
        visited[root] = true;
        std::vector<std::pair<std::size_t, std::size_t>> stack{{root, 0}};
        while (!stack.empty()) {
            auto &[node, next] = stack.back();
            if (next < forward[node].size()) {
                const std::size_t target = forward[node][next++];
                if (!visited[target]) {
                    visited[target] = true;
                    stack.emplace_back(target, 0);
                }
            } else {
                finishOrder.push_back(node);
                stack.pop_back();
            }
        }
    }

    std::fill(visited.begin(), visited.end(), false);
    std::vector<GrammarCycleComponent> result;
    for (auto it = finishOrder.rbegin(); it != finishOrder.rend(); ++it) {
        const std::size_t root = *it;
        if (visited[root]) {
            continue;
        }
        std::vector<std::size_t> stack{root};
        std::vector<NonterminalId> component;
        visited[root] = true;
        while (!stack.empty()) {
            const std::size_t node = stack.back();
            stack.pop_back();
            component.push_back(NonterminalId{static_cast<std::uint32_t>(node)});
            for (const std::size_t target: reverse[node]) {
                if (!visited[target]) {
                    visited[target] = true;
                    stack.push_back(target);
                }
            }
        }
        std::ranges::sort(component);
        bool cyclic = component.size() > 1;
        if (!cyclic) {
            cyclic = std::ranges::any_of(edges, [&](const auto &edge) {
                return edge.from == component.front() && edge.to == component.front();
            });
        }
        if (cyclic) {
            result.push_back({component, cycleWitness(component, edges)});
        }
    }
    std::ranges::sort(result, {}, &GrammarCycleComponent::nonterminals);
    return result;
}

} // namespace

GrammarAnalysis::GrammarAnalysis(const Grammar &grammar)
    : grammar_(grammar), nullable_(grammar),
      productiveNonterminals_(grammar.nonterminalCount(), false),
      productiveRules_(grammar.ruleCount(), false),
      reachableNonterminals_(grammar.nonterminalCount(), false),
      reachableRules_(grammar.ruleCount(), false),
      usefulNonterminals_(grammar.nonterminalCount(), false),
      usefulRules_(grammar.ruleCount(), false),
      nonNullableCounts_(grammar.ruleCount(), 0) {
    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            if (productiveRules_[toIndex(rule.id())]) {
                continue;
            }
            if (std::ranges::all_of(rule.rhs(), [&](const auto &symbol) {
                    return productiveSymbol(symbol, productiveNonterminals_);
                })) {
                productiveRules_[toIndex(rule.id())] = true;
                productiveNonterminals_[toIndex(rule.lhs())] = true;
                changed = true;
            }
        }
    } while (changed);

    std::queue<NonterminalId> reachable;
    reachableNonterminals_[toIndex(grammar.start())] = true;
    reachable.push(grammar.start());
    while (!reachable.empty()) {
        const NonterminalId lhs = reachable.front();
        reachable.pop();
        for (const RuleId ruleId: grammar.rulesFor(lhs)) {
            reachableRules_[toIndex(ruleId)] = true;
            for (const auto &symbol: grammar.rule(ruleId).rhs()) {
                if (const auto nonterminal = std::get_if<NonterminalId>(&symbol);
                        nonterminal && !reachableNonterminals_[toIndex(*nonterminal)]) {
                    reachableNonterminals_[toIndex(*nonterminal)] = true;
                    reachable.push(*nonterminal);
                }
            }
        }
    }

    if (productiveNonterminals_[toIndex(grammar.start())]) {
        std::queue<NonterminalId> useful;
        usefulNonterminals_[toIndex(grammar.start())] = true;
        useful.push(grammar.start());
        while (!useful.empty()) {
            const NonterminalId lhs = useful.front();
            useful.pop();
            for (const RuleId ruleId: grammar.rulesFor(lhs)) {
                if (!productiveRules_[toIndex(ruleId)]) {
                    continue;
                }
                usefulRules_[toIndex(ruleId)] = true;
                for (const auto &symbol: grammar.rule(ruleId).rhs()) {
                    if (const auto nonterminal = std::get_if<NonterminalId>(&symbol);
                            nonterminal && !usefulNonterminals_[toIndex(*nonterminal)]) {
                        usefulNonterminals_[toIndex(*nonterminal)] = true;
                        useful.push(*nonterminal);
                    }
                }
            }
        }
    }

    for (const Rule &rule: grammar.rules()) {
        std::size_t count = 0;
        for (const auto &symbol: rule.rhs()) {
            count += nullable_.isNullable(symbol) ? 0U : 1U;
        }
        nonNullableCounts_[toIndex(rule.id())] = count;

        std::vector<bool> prefix(rule.size() + 1, true);
        std::vector<bool> suffix(rule.size() + 1, true);
        for (std::size_t i = 0; i < rule.size(); ++i) {
            prefix[i + 1] = prefix[i] && nullable_.isNullable(rule.symbol(i));
        }
        for (std::size_t i = rule.size(); i > 0; --i) {
            suffix[i - 1] = suffix[i] && nullable_.isNullable(rule.symbol(i - 1));
        }
        for (std::size_t i = 0; i < rule.size(); ++i) {
            const auto target = std::get_if<NonterminalId>(&rule.symbol(i));
            if (!target) {
                continue;
            }
            const GrammarDependencyEdge edge{rule.lhs(), *target, rule.id(), i};
            if (prefix[i]) {
                leftCornerEdges_.push_back(edge);
            }
            if (prefix[i] && suffix[i + 1]) {
                zeroProgressEdges_.push_back(edge);
            }
        }
    }
    std::ranges::sort(leftCornerEdges_);
    std::ranges::sort(zeroProgressEdges_);
    leftRecursiveCycles_ = cyclicComponents(grammar.nonterminalCount(), leftCornerEdges_);
    zeroProgressCycles_ = cyclicComponents(grammar.nonterminalCount(), zeroProgressEdges_);
}

const Grammar &GrammarAnalysis::grammar() const noexcept { return grammar_; }

bool GrammarAnalysis::isProductive(NonterminalId id) const {
    return productiveNonterminals_.at(toIndex(id));
}

bool GrammarAnalysis::isProductive(RuleId id) const {
    return productiveRules_.at(toIndex(id));
}

bool GrammarAnalysis::isReachable(NonterminalId id) const {
    return reachableNonterminals_.at(toIndex(id));
}

bool GrammarAnalysis::isReachable(RuleId id) const {
    return reachableRules_.at(toIndex(id));
}

bool GrammarAnalysis::isUseful(NonterminalId id) const {
    return usefulNonterminals_.at(toIndex(id));
}

bool GrammarAnalysis::isUseful(RuleId id) const {
    return usefulRules_.at(toIndex(id));
}

std::size_t GrammarAnalysis::nonNullableCount(RuleId id) const {
    return nonNullableCounts_.at(toIndex(id));
}

std::span<const GrammarDependencyEdge> GrammarAnalysis::zeroProgressEdges() const noexcept {
    return zeroProgressEdges_;
}

std::span<const GrammarCycleComponent> GrammarAnalysis::zeroProgressCycles() const noexcept {
    return zeroProgressCycles_;
}

std::span<const GrammarDependencyEdge> GrammarAnalysis::leftCornerEdges() const noexcept {
    return leftCornerEdges_;
}

std::span<const GrammarCycleComponent> GrammarAnalysis::leftRecursiveCycles() const noexcept {
    return leftRecursiveCycles_;
}

} // namespace zbik
