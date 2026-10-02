#include "DirectLALRkDfa.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>

#include "LRGrammarView.h"

namespace zbik {
namespace {

using CoreSet = std::vector<ItemCore>;

struct CoreState {
    CoreSet cores;
    std::map<SymbolRef, StateId> transitions;
};

using Lookaheads = std::map<ItemCore, WordSetK>;

CoreSet closeCores(const LRGrammarView &grammar, std::set<ItemCore> cores) {
    std::vector<ItemCore> pending(cores.begin(), cores.end());
    for (std::size_t next = 0; next < pending.size(); ++next) {
        const ItemCore item = pending[next];
        const Rule &rule = grammar.rule(item.rule);
        if (item.dot == rule.size()) continue;
        const auto nonterminal = std::get_if<NonterminalId>(&rule.symbol(item.dot));
        if (nonterminal == nullptr) continue;
        for (const RuleId child: grammar.rulesFor(*nonterminal)) {
            const ItemCore childItem{child, 0};
            if (cores.insert(childItem).second) pending.push_back(childItem);
        }
    }
    return {cores.begin(), cores.end()};
}

std::vector<CoreState> buildCoreGraph(const LRGrammarView &grammar) {
    std::vector<CoreState> states;
    std::map<CoreSet, StateId> ids;
    const auto intern = [&](CoreSet cores) {
        const auto [position, inserted] = ids.emplace(cores, StateId{states.size()});
        if (inserted) states.push_back({std::move(cores), {}});
        return position->second;
    };

    intern(closeCores(grammar, {{grammar.syntheticRuleId(), 0}}));
    for (std::size_t index = 0; index < states.size(); ++index) {
        std::map<SymbolRef, std::set<ItemCore>> kernels;
        for (const ItemCore item: states[index].cores) {
            const Rule &rule = grammar.rule(item.rule);
            if (item.dot < rule.size()) {
                kernels[rule.symbol(item.dot)].insert({item.rule, item.dot + 1});
            }
        }
        for (auto &[symbol, kernel]: kernels) {
            const StateId target = intern(closeCores(grammar, std::move(kernel)));
            states[index].transitions.emplace(symbol, target);
        }
    }
    return states;
}

bool addLookahead(
        Lookaheads &lookaheads, ItemCore core,
        const LookaheadWord &word, std::size_t maxLength) {
    auto [position, inserted] = lookaheads.try_emplace(core, maxLength);
    static_cast<void>(inserted);
    return position->second.add(word);
}

std::vector<Lookaheads> propagateLookaheads(
        const LRGrammarView &grammar, const FirstKAnalysis &first,
        const std::vector<CoreState> &states) {
    struct Pending {
        StateId state;
        ItemCore core;
        LookaheadWord word;
    };

    std::vector<Lookaheads> result(states.size());
    std::queue<Pending> pending;
    const auto enqueue = [&](StateId state, ItemCore core,
                             const LookaheadWord &word) {
        if (addLookahead(result.at(toIndex(state)), core, word,
                         first.maxLength())) {
            pending.push({state, core, word});
        }
    };
    enqueue(StateId{0}, {grammar.syntheticRuleId(), 0},
            LookaheadWord{{endOfInput}});

    while (!pending.empty()) {
        Pending current = std::move(pending.front());
        pending.pop();
        const Rule &rule = grammar.rule(current.core.rule);
        if (current.core.dot == rule.size()) continue;

        if (const auto nonterminal =
                    std::get_if<NonterminalId>(&rule.symbol(current.core.dot))) {
            const std::span<const SymbolRef> suffix{
                    rule.rhs().begin() + current.core.dot + 1, rule.rhs().end()};
            const WordSetK closureWords = first.first(suffix, current.word);
            for (const RuleId child: grammar.rulesFor(*nonterminal)) {
                for (const LookaheadWord &closureWord: closureWords.words()) {
                    enqueue(current.state, {child, 0}, closureWord);
                }
            }
        }

        const SymbolRef &symbol = rule.symbol(current.core.dot);
        const auto transition =
                states[toIndex(current.state)].transitions.find(symbol);
        if (transition == states[toIndex(current.state)].transitions.end()) {
            throw std::logic_error("LR(0) core item has no transition");
        }
        enqueue(transition->second,
                {current.core.rule, current.core.dot + 1}, current.word);
    }
    return result;
}

bool activeTransition(
        const LRGrammarView &grammar, const CoreState &state,
        const Lookaheads &lookaheads, const SymbolRef &symbol) {
    return std::ranges::any_of(state.cores, [&](const ItemCore item) {
        const Rule &rule = grammar.rule(item.rule);
        const auto words = lookaheads.find(item);
        return item.dot < rule.size() && rule.symbol(item.dot) == symbol &&
               words != lookaheads.end() && !words->second.empty();
    });
}

std::vector<LRkState> materialize(
        const LRGrammarView &grammar, const std::vector<CoreState> &coreStates,
        const std::vector<Lookaheads> &lookaheads) {
    std::vector<std::optional<StateId>> remapped(coreStates.size());
    std::vector<StateId> order;
    std::queue<StateId> pending;
    remapped.front() = StateId{0};
    order.push_back(StateId{0});
    pending.push(StateId{0});

    while (!pending.empty()) {
        const StateId source = pending.front();
        pending.pop();
        for (const auto &[symbol, target]: coreStates[toIndex(source)].transitions) {
            if (!activeTransition(grammar, coreStates[toIndex(source)],
                                  lookaheads[toIndex(source)], symbol)) {
                continue;
            }
            if (!remapped[toIndex(target)].has_value()) {
                remapped[toIndex(target)] = StateId{order.size()};
                order.push_back(target);
                pending.push(target);
            }
        }
    }

    std::vector<LRkState> result(order.size());
    for (std::size_t newIndex = 0; newIndex < order.size(); ++newIndex) {
        const StateId oldId = order[newIndex];
        LRkState &targetState = result[newIndex];
        for (const auto &[core, words]: lookaheads[toIndex(oldId)]) {
            for (const LookaheadWord &word: words.words()) {
                targetState.items.emplace_back(
                        grammar, core.rule, core.dot, word);
            }
        }
        for (const auto &[symbol, oldTarget]: coreStates[toIndex(oldId)].transitions) {
            if (!activeTransition(grammar, coreStates[toIndex(oldId)],
                                  lookaheads[toIndex(oldId)], symbol)) {
                continue;
            }
            targetState.transitions.emplace(symbol, *remapped[toIndex(oldTarget)]);
        }
    }
    return result;
}

} // namespace

LRkDfa DirectLALRkDfa::build(
        const Grammar &grammar, const FirstKAnalysis &first) {
    if (&first.grammar() != &grammar) {
        throw std::invalid_argument(
                "direct LALR(k) requires FIRST(k) for the same grammar");
    }
    if (first.maxLength() == 0) {
        throw std::invalid_argument("direct LALR(k) requires k >= 1");
    }
    const LRGrammarView view(grammar);
    const std::vector<CoreState> coreStates = buildCoreGraph(view);
    const std::vector<Lookaheads> lookaheads =
            propagateLookaheads(view, first, coreStates);
    return LRkDfa(grammar, first.maxLength(),
                  materialize(view, coreStates, lookaheads), "LALR");
}

DirectLALRkDfa::DirectLALRkDfa(
        const Grammar &grammar, std::size_t maxLength)
    : DirectLALRkDfa(grammar, FirstKAnalysis(grammar, maxLength)) {}

DirectLALRkDfa::DirectLALRkDfa(
        const Grammar &grammar, const FirstKAnalysis &first)
    : graph_(build(grammar, first)) {}

} // namespace zbik
