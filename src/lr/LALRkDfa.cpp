#include "LALRkDfa.h"

#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace zbik {

struct LALRkDfa::MergeResult {
    std::vector<LRkState> states;
    std::vector<std::vector<StateId>> origins;
};

namespace {

std::vector<ItemCore> stateCore(const LRkState &state) {
    std::vector<ItemCore> result;
    for (const Item &item: state.items) {
        if (result.empty() || result.back() != item.core()) {
            result.push_back(item.core());
        }
    }
    return result;
}

} // namespace

LALRkDfa::LALRkDfa(
        const Grammar &grammar, std::size_t maxLength, StateHasher hasher)
    : LALRkDfa(LRkDfa(grammar, maxLength, std::move(hasher))) {}

LALRkDfa::LALRkDfa(const LRkDfa &canonical)
    : LALRkDfa(canonical, merge(canonical)) {}

LALRkDfa::LALRkDfa(const LRkDfa &canonical, MergeResult merged)
    : graph_(canonical.grammar(), canonical.maxLength(),
             std::move(merged.states), "LALR"),
      canonical_(canonical), origins_(std::move(merged.origins)) {}

const LRkDfa &LALRkDfa::canonical() const noexcept {
    return canonical_;
}

std::span<const StateId> LALRkDfa::canonicalOrigins(StateId state) const {
    return origins_.at(toIndex(state));
}

std::span<const std::vector<StateId>> LALRkDfa::originGroups() const noexcept {
    return origins_;
}

LALRkDfa::MergeResult LALRkDfa::merge(const LRkDfa &canonical) {
    MergeResult result;
    std::map<std::vector<ItemCore>, StateId> groups;
    std::vector<StateId> mergedState(canonical.states().size());
    std::vector<std::set<Item>> itemSets;

    for (std::size_t index = 0; index < canonical.states().size(); ++index) {
        const LRkState &source = canonical.state(StateId{index});
        const auto [position, inserted] = groups.emplace(
                stateCore(source), StateId{result.states.size()});
        if (inserted) {
            result.states.emplace_back();
            result.origins.emplace_back();
            itemSets.emplace_back();
        }
        const StateId target = position->second;
        mergedState[index] = target;
        result.origins[toIndex(target)].push_back(StateId{index});
        itemSets[toIndex(target)].insert(source.items.begin(), source.items.end());
    }

    for (std::size_t index = 0; index < result.states.size(); ++index) {
        result.states[index].items.assign(
                itemSets[index].begin(), itemSets[index].end());
    }

    for (std::size_t index = 0; index < canonical.states().size(); ++index) {
        const StateId source = mergedState[index];
        for (const auto &[symbol, canonicalTarget]:
                canonical.state(StateId{index}).transitions) {
            const StateId target = mergedState.at(toIndex(canonicalTarget));
            const auto [position, inserted] =
                    result.states[toIndex(source)].transitions.emplace(symbol, target);
            if (!inserted && position->second != target) {
                throw std::logic_error("equal LR cores have inconsistent transitions");
            }
        }
    }
    return result;
}

} // namespace zbik
