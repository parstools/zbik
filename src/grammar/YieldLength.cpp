#include "YieldLength.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "GrammarAnalysis.h"

namespace zbik {
namespace {

MinYield addMinimum(MinYield left, MinYield right) {
    if (!left.hasYield() || !right.hasYield()) {
        return MinYield::noYield();
    }
    if (left.exceeds() || right.exceeds()) {
        return MinYield::exceedsSizeT();
    }
    if (right.value() > std::numeric_limits<std::size_t>::max() - left.value()) {
        return MinYield::exceedsSizeT();
    }
    return MinYield::finite(left.value() + right.value());
}

MinYield minimumOf(MinYield left, MinYield right) {
    if (!left.hasYield()) {
        return right;
    }
    if (!right.hasYield()) {
        return left;
    }
    if (left.exceeds()) {
        return right;
    }
    if (right.exceeds()) {
        return left;
    }
    return MinYield::finite(std::min(left.value(), right.value()));
}

MinYield ruleMinimum(const Rule &rule, std::span<const MinYield> minima) {
    MinYield result = MinYield::finite(0);
    for (const SymbolRef &symbol: rule.rhs()) {
        const MinYield part = std::holds_alternative<TerminalId>(symbol)
                ? MinYield::finite(1)
                : minima[toIndex(std::get<NonterminalId>(symbol))];
        result = addMinimum(result, part);
    }
    return result;
}

MaxYield addMaximum(MaxYield left, MaxYield right) {
    if (!left.hasYield() || !right.hasYield()) {
        return MaxYield::noYield();
    }
    if (left.isUnbounded() || right.isUnbounded()) {
        return MaxYield::unbounded();
    }
    if (left.exceeds() || right.exceeds()) {
        return MaxYield::exceedsSizeT();
    }
    if (right.value() > std::numeric_limits<std::size_t>::max() - left.value()) {
        return MaxYield::exceedsSizeT();
    }
    return MaxYield::finite(left.value() + right.value());
}

MaxYield maximumOf(MaxYield left, MaxYield right) {
    if (left.isUnbounded() || right.isUnbounded()) {
        return MaxYield::unbounded();
    }
    if (left.exceeds() || right.exceeds()) {
        return MaxYield::exceedsSizeT();
    }
    if (!left.hasYield()) {
        return right;
    }
    if (!right.hasYield()) {
        return left;
    }
    return MaxYield::finite(std::max(left.value(), right.value()));
}

MaxYield ruleMaximum(const Rule &rule, std::span<const MaxYield> maxima) {
    MaxYield result = MaxYield::finite(0);
    for (const SymbolRef &symbol: rule.rhs()) {
        const MaxYield part = std::holds_alternative<TerminalId>(symbol)
                ? MaxYield::finite(1)
                : maxima[toIndex(std::get<NonterminalId>(symbol))];
        result = addMaximum(result, part);
    }
    return result;
}

struct LengthEdge {
    std::size_t from;
    std::size_t to;
    bool expanding;
};

std::vector<std::vector<std::size_t>> adjacency(
        std::size_t count, const std::vector<LengthEdge> &edges, bool reverse) {
    std::vector<std::vector<std::size_t>> result(count);
    for (const auto &edge: edges) {
        result[reverse ? edge.to : edge.from].push_back(
                reverse ? edge.from : edge.to);
    }
    for (auto &row: result) {
        std::ranges::sort(row);
        row.erase(std::unique(row.begin(), row.end()), row.end());
    }
    return result;
}

std::vector<std::vector<std::size_t>> stronglyConnectedComponents(
        std::size_t count, const std::vector<LengthEdge> &edges) {
    const auto forward = adjacency(count, edges, false);
    const auto reverse = adjacency(count, edges, true);
    std::vector<bool> visited(count, false);
    std::vector<std::size_t> order;
    for (std::size_t root = 0; root < count; ++root) {
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
                order.push_back(node);
                stack.pop_back();
            }
        }
    }

    std::fill(visited.begin(), visited.end(), false);
    std::vector<std::vector<std::size_t>> result;
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        if (visited[*it]) {
            continue;
        }
        result.emplace_back();
        std::vector<std::size_t> stack{*it};
        visited[*it] = true;
        while (!stack.empty()) {
            const std::size_t node = stack.back();
            stack.pop_back();
            result.back().push_back(node);
            for (const std::size_t target: reverse[node]) {
                if (!visited[target]) {
                    visited[target] = true;
                    stack.push_back(target);
                }
            }
        }
        std::ranges::sort(result.back());
    }
    return result;
}

} // namespace

MinYield::MinYield(Kind kind, std::size_t value) noexcept
    : kind_(kind), value_(value) {}
MinYield MinYield::finite(std::size_t value) noexcept { return {Kind::Finite, value}; }
MinYield MinYield::exceedsSizeT() noexcept { return {Kind::ExceedsSizeT, 0}; }
MinYield MinYield::noYield() noexcept { return {Kind::NoYield, 0}; }
MinYield::Kind MinYield::kind() const noexcept { return kind_; }
bool MinYield::isFinite() const noexcept { return kind_ == Kind::Finite; }
bool MinYield::exceeds() const noexcept { return kind_ == Kind::ExceedsSizeT; }
bool MinYield::hasYield() const noexcept { return kind_ != Kind::NoYield; }
std::size_t MinYield::value() const {
    if (!isFinite()) {
        throw std::logic_error("a non-finite minimum has no size_t value");
    }
    return value_;
}

MaxYield::MaxYield(Kind kind, std::size_t value) noexcept
    : kind_(kind), value_(value) {}
MaxYield MaxYield::noYield() noexcept { return {Kind::NoYield, 0}; }
MaxYield MaxYield::finite(std::size_t value) noexcept { return {Kind::Finite, value}; }
MaxYield MaxYield::exceedsSizeT() noexcept { return {Kind::ExceedsSizeT, 0}; }
MaxYield MaxYield::unbounded() noexcept { return {Kind::Unbounded, 0}; }
MaxYield::Kind MaxYield::kind() const noexcept { return kind_; }
bool MaxYield::isFinite() const noexcept { return kind_ == Kind::Finite; }
bool MaxYield::exceeds() const noexcept { return kind_ == Kind::ExceedsSizeT; }
bool MaxYield::hasYield() const noexcept { return kind_ != Kind::NoYield; }
bool MaxYield::isUnbounded() const noexcept { return kind_ == Kind::Unbounded; }
std::size_t MaxYield::value() const {
    if (!isFinite()) {
        throw std::logic_error("a non-finite maximum has no size_t value");
    }
    return value_;
}

MinYieldLength::MinYieldLength(const Grammar &grammar)
    : grammar_(grammar),
      nonterminalMinima_(grammar.nonterminalCount(), MinYield::noYield()),
      ruleMinima_(grammar.ruleCount(), MinYield::noYield()) {
    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            const MinYield candidate = ruleMinimum(rule, nonterminalMinima_);
            const std::size_t lhs = toIndex(rule.lhs());
            const MinYield combined = minimumOf(nonterminalMinima_[lhs], candidate);
            if (combined != nonterminalMinima_[lhs]) {
                nonterminalMinima_[lhs] = combined;
                changed = true;
            }
        }
    } while (changed);

    for (const Rule &rule: grammar.rules()) {
        ruleMinima_[toIndex(rule.id())] = ruleMinimum(rule, nonterminalMinima_);
    }
    for (std::size_t i = 0; i < nonterminalMinima_.size(); ++i) {
        const NonterminalId id{static_cast<std::uint32_t>(i)};
        if (!nonterminalMinima_[i].hasYield()) noYieldNonterminals_.push_back(id);
        if (nonterminalMinima_[i].exceeds()) exceededNonterminals_.push_back(id);
    }
    for (std::size_t i = 0; i < ruleMinima_.size(); ++i) {
        const RuleId id{static_cast<std::uint32_t>(i)};
        if (!ruleMinima_[i].hasYield()) noYieldRules_.push_back(id);
        if (ruleMinima_[i].exceeds()) exceededRules_.push_back(id);
    }
}

const Grammar &MinYieldLength::grammar() const noexcept { return grammar_; }
MinYield MinYieldLength::minimum(TerminalId terminal) const {
    static_cast<void>(grammar_.terminalName(terminal));
    return MinYield::finite(1);
}
MinYield MinYieldLength::minimum(NonterminalId id) const {
    return nonterminalMinima_.at(toIndex(id));
}
MinYield MinYieldLength::minimum(const SymbolRef &symbol) const {
    return std::visit([this](auto id) { return minimum(id); }, symbol);
}
MinYield MinYieldLength::minimum(RuleId id) const {
    return ruleMinima_.at(toIndex(id));
}
bool MinYieldLength::checkMinLen() const noexcept {
    return noYieldNonterminals_.empty() && noYieldRules_.empty();
}
std::span<const NonterminalId> MinYieldLength::noYieldNonterminals() const noexcept {
    return noYieldNonterminals_;
}
std::span<const RuleId> MinYieldLength::noYieldRules() const noexcept {
    return noYieldRules_;
}
std::span<const NonterminalId> MinYieldLength::exceededNonterminals() const noexcept {
    return exceededNonterminals_;
}
std::span<const RuleId> MinYieldLength::exceededRules() const noexcept {
    return exceededRules_;
}

MaxYieldLength::MaxYieldLength(const Grammar &grammar)
    : grammar_(grammar),
      nonterminalMaxima_(grammar.nonterminalCount(), MaxYield::noYield()),
      ruleMaxima_(grammar.ruleCount(), MaxYield::noYield()) {
    const GrammarAnalysis analysis(grammar);
    std::vector<bool> positive(grammar.nonterminalCount(), false);
    bool changed;
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            if (!analysis.isProductive(rule.id())) {
                continue;
            }
            const bool rulePositive = std::ranges::any_of(rule.rhs(), [&](const auto &symbol) {
                if (std::holds_alternative<TerminalId>(symbol)) return true;
                return static_cast<bool>(
                        positive[toIndex(std::get<NonterminalId>(symbol))]);
            });
            const std::size_t lhs = toIndex(rule.lhs());
            if (rulePositive && !positive[lhs]) {
                positive[lhs] = true;
                changed = true;
            }
        }
    } while (changed);

    std::vector<LengthEdge> edges;
    for (const Rule &rule: grammar.rules()) {
        if (!analysis.isProductive(rule.id())) {
            continue;
        }
        for (std::size_t position = 0; position < rule.size(); ++position) {
            const auto target = std::get_if<NonterminalId>(&rule.symbol(position));
            if (!target) {
                continue;
            }
            bool expanding = false;
            for (std::size_t other = 0; other < rule.size(); ++other) {
                if (other == position) continue;
                const SymbolRef &symbol = rule.symbol(other);
                expanding |= std::holds_alternative<TerminalId>(symbol)
                        || positive[toIndex(std::get<NonterminalId>(symbol))];
            }
            edges.push_back({toIndex(rule.lhs()), toIndex(*target), expanding});
        }
    }

    std::vector<bool> unbounded(grammar.nonterminalCount(), false);
    for (const auto &component:
            stronglyConnectedComponents(grammar.nonterminalCount(), edges)) {
        std::vector<bool> inside(grammar.nonterminalCount(), false);
        for (const std::size_t node: component) inside[node] = true;
        const bool expandingCycle = std::ranges::any_of(edges, [&](const auto &edge) {
            return edge.expanding && inside[edge.from] && inside[edge.to];
        });
        if (expandingCycle) {
            for (const std::size_t node: component) unbounded[node] = true;
        }
    }

    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            if (!analysis.isProductive(rule.id())) continue;
            const bool reachesUnbounded = std::ranges::any_of(
                    rule.rhs(), [&](const auto &symbol) {
                const auto id = std::get_if<NonterminalId>(&symbol);
                return id && unbounded[toIndex(*id)];
            });
            const std::size_t lhs = toIndex(rule.lhs());
            if (reachesUnbounded && !unbounded[lhs]) {
                unbounded[lhs] = true;
                changed = true;
            }
        }
    } while (changed);

    for (std::size_t i = 0; i < unbounded.size(); ++i) {
        if (unbounded[i]) nonterminalMaxima_[i] = MaxYield::unbounded();
    }
    do {
        changed = false;
        for (const Rule &rule: grammar.rules()) {
            const MaxYield candidate = ruleMaximum(rule, nonterminalMaxima_);
            const std::size_t lhs = toIndex(rule.lhs());
            const MaxYield combined = maximumOf(nonterminalMaxima_[lhs], candidate);
            if (combined != nonterminalMaxima_[lhs]) {
                nonterminalMaxima_[lhs] = combined;
                changed = true;
            }
        }
    } while (changed);
    for (const Rule &rule: grammar.rules()) {
        ruleMaxima_[toIndex(rule.id())] = ruleMaximum(rule, nonterminalMaxima_);
    }
}

const Grammar &MaxYieldLength::grammar() const noexcept { return grammar_; }
MaxYield MaxYieldLength::maximum(TerminalId terminal) const {
    static_cast<void>(grammar_.terminalName(terminal));
    return MaxYield::finite(1);
}
MaxYield MaxYieldLength::maximum(NonterminalId id) const {
    return nonterminalMaxima_.at(toIndex(id));
}
MaxYield MaxYieldLength::maximum(const SymbolRef &symbol) const {
    return std::visit([this](auto id) { return maximum(id); }, symbol);
}
MaxYield MaxYieldLength::maximum(RuleId id) const {
    return ruleMaxima_.at(toIndex(id));
}

} // namespace zbik
