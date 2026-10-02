#include "regex/RegexDfa.h"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zbik {

RegexDfa::RegexDfa(const RegexNfa &nfa, StateHasher hasher) {
    if (!hasher) {
        throw std::invalid_argument("RegexDfa requires a state hasher");
    }

    std::unordered_map<std::size_t, std::vector<DfaStateId>> buckets;
    const auto intern = [&](NfaStateSubset subset) -> DfaStateId {
        auto &bucket = buckets[hasher(subset)];
        for (const DfaStateId id: bucket) {
            if (states_[toIndex(id)].nfaStates == subset) return id;
        }
        if (states_.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("DFA has too many states");
        }
        const DfaStateId id{static_cast<std::uint32_t>(states_.size())};
        const bool accepting = std::ranges::binary_search(
                subset, nfa.acceptingState());
        states_.push_back({std::move(subset), accepting, {}});
        bucket.push_back(id);
        return id;
    };

    const DfaStateId start = intern(nfa.epsilonClosure({nfa.startState()}));
    if (start != DfaStateId{0}) {
        throw std::logic_error("The first interned DFA state must be the start state");
    }

    for (std::size_t index = 0; index < states_.size(); ++index) {
        // Interning targets may reallocate states_, so keep the source subset by value.
        const NfaStateSubset sourceSubset = states_[index].nfaStates;
        std::vector<CodePoint> boundaries;
        for (const NfaStateId nfaStateId: sourceSubset) {
            for (const NfaByteTransition &transition:
                 nfa.state(nfaStateId).byteTransitions) {
                for (const CodePointRange range: transition.bytes.ranges()) {
                    boundaries.push_back(range.first);
                    boundaries.push_back(range.last + 1U);
                }
            }
        }
        std::ranges::sort(boundaries);
        const auto uniqueEnd = std::ranges::unique(boundaries).begin();
        boundaries.erase(uniqueEnd, boundaries.end());

        std::map<DfaStateId, std::vector<CodePointRange>> rangesByTarget;
        for (std::size_t boundary = 0; boundary + 1 < boundaries.size();
             ++boundary) {
            const CodePoint first = boundaries[boundary];
            const CodePoint last = boundaries[boundary + 1] - 1U;
            std::vector<NfaStateId> targets;
            for (const NfaStateId nfaStateId: sourceSubset) {
                for (const NfaByteTransition &transition:
                     nfa.state(nfaStateId).byteTransitions) {
                    if (transition.bytes.contains(first)) {
                        targets.push_back(transition.target);
                    }
                }
            }
            if (targets.empty()) continue;
            std::ranges::sort(targets);
            targets.erase(std::ranges::unique(targets).begin(), targets.end());
            const DfaStateId target = intern(nfa.epsilonClosure(targets));
            auto &ranges = rangesByTarget[target];
            if (!ranges.empty() && ranges.back().last + 1U == first) {
                ranges.back().last = last;
            } else {
                ranges.push_back({first, last});
            }
        }

        std::vector<DfaByteTransition> transitions;
        transitions.reserve(rangesByTarget.size());
        for (auto &[target, ranges]: rangesByTarget) {
            transitions.push_back(
                    {CodePointClass(std::move(ranges)), target});
        }
        states_[index].byteTransitions = std::move(transitions);
    }
}

DfaStateId RegexDfa::startState() const noexcept {
    return {0};
}

std::span<const DfaState> RegexDfa::states() const noexcept {
    return states_;
}

const DfaState &RegexDfa::state(DfaStateId id) const {
    if (toIndex(id) >= states_.size()) {
        throw std::out_of_range("Unknown DFA state");
    }
    return states_[toIndex(id)];
}

std::size_t RegexDfa::hashSubset(const NfaStateSubset &subset) {
    std::size_t seed = subset.size();
    for (const NfaStateId id: subset) {
        const std::size_t value = std::hash<NfaStateId>{}(id);
        seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    }
    return seed;
}

} // namespace zbik
