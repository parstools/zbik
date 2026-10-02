#include "LRkDfa.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "LRkGoto.h"
#include "ParseTable.h"

namespace zbik {
namespace {

struct GraphBuildResult {
    std::vector<LRkState> states;
    std::size_t completedStates = 0;
    bool conflict = false;
};

GraphBuildResult buildGraph(
        const Grammar &grammar, const FirstKAnalysis &first,
        const LRkDfa::StateHasher &hasher, bool stopAfterFirstConflict) {
    const LRGrammarView view(grammar);
    const LRkClosure closure(view, first);
    const LRkGoto goTo(closure);
    GraphBuildResult result;
    std::unordered_map<std::size_t, std::vector<StateId>> buckets;
    const auto intern = [&](ItemSet items) -> StateId {
        auto &bucket = buckets[hasher(items)];
        for (const StateId id: bucket) {
            if (result.states[id.value].items == items) return id;
        }
        const StateId id{result.states.size()};
        result.states.push_back({std::move(items), {}});
        bucket.push_back(id);
        return id;
    };

    intern(closure.close(Item(
            view, view.syntheticRuleId(), 0,
            LookaheadWord{{endOfInput}})));
    for (std::size_t index = 0; index < result.states.size(); ++index) {
        std::set<SymbolRef> symbols;
        for (const Item &item: result.states[index].items) {
            const Rule &rule = view.rule(item.rule);
            if (item.dot < rule.size()) symbols.insert(rule.symbol(item.dot));
        }
        for (const SymbolRef &symbol: symbols) {
            ItemSet target = goTo.goTo(result.states[index].items, symbol);
            if (!target.empty()) {
                const StateId destination = intern(std::move(target));
                // Interning may reallocate states; reacquire the source by ID.
                result.states[index].transitions.emplace(symbol, destination);
            }
        }
        ++result.completedStates;

        if (stopAfterFirstConflict) {
            const ActionRow row = buildActionRow(view, first, result.states[index]);
            if (std::ranges::any_of(row, [](const auto &entry) {
                    return entry.second.hasConflict();
                })) {
                result.conflict = true;
                break;
            }
        }
    }
    return result;
}

void validateInputs(
        const Grammar &grammar, const FirstKAnalysis &first,
        const LRkDfa::StateHasher &hasher) {
    if (&first.grammar() != &grammar) {
        throw std::invalid_argument("LRkDfa requires FIRST(k) for the same grammar");
    }
    if (first.maxLength() == 0 || !hasher) {
        throw std::invalid_argument("LRkDfa requires k >= 1 and a state hasher");
    }
}

} // namespace

LRkDfa::LRkDfa(const Grammar &grammar, std::size_t maxLength, StateHasher hasher)
    : LRkDfa(grammar, FirstKAnalysis(grammar, maxLength), std::move(hasher)) {}

LRkDfa::LRkDfa(
        const Grammar &grammar, const FirstKAnalysis &first, StateHasher hasher)
    : grammar_(grammar), maxLength_(first.maxLength()), name_("LR") {
    validateInputs(grammar, first, hasher);
    states_ = buildGraph(grammar, first, hasher, false).states;
}

LRkBuildUntilConflictResult LRkDfa::buildUntilFirstConflict(
        const Grammar &grammar, const FirstKAnalysis &first,
        StateHasher hasher) {
    validateInputs(grammar, first, hasher);
    GraphBuildResult graph = buildGraph(grammar, first, hasher, true);
    LRkBuildUntilConflictResult result{};
    result.conflict = graph.conflict;
    result.completedStates = graph.completedStates;
    result.discoveredStates = graph.states.size();
    if (!graph.conflict) {
        result.dfa.emplace(LRkDfa(
                Grammar{grammar}, first.maxLength(), std::move(graph.states), "LR"));
    }
    return result;
}

LRkDfa::LRkDfa(
        Grammar grammar, std::size_t maxLength, std::vector<LRkState> states,
        std::string name)
    : grammar_(std::move(grammar)), maxLength_(maxLength),
      states_(std::move(states)), name_(std::move(name)) {
    if (maxLength == 0 || states_.empty() || name_.empty()) {
        throw std::invalid_argument("an LR automaton needs k >= 1 and a start state");
    }
}

std::size_t LRkDfa::maxLength() const noexcept { return maxLength_; }
const Grammar &LRkDfa::grammar() const noexcept { return grammar_; }
StateId LRkDfa::start() const noexcept { return {0}; }
std::span<const LRkState> LRkDfa::states() const noexcept { return states_; }
const LRkState &LRkDfa::state(StateId id) const { return states_.at(id.value); }

std::size_t LRkDfa::hashItems(const ItemSet &items) {
    std::size_t seed = items.size();
    const auto combine = [&seed](std::size_t value) {
        seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    };
    for (const Item &item: items) {
        combine(std::hash<RuleId>{}(item.rule));
        combine(std::hash<std::size_t>{}(item.dot));
        combine(LookaheadWordHash{}(item.lookahead));
    }
    return seed;
}

} // namespace zbik
