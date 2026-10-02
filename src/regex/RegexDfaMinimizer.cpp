#include "regex/RegexDfaMinimizer.h"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace zbik {
namespace {

constexpr std::size_t missing = std::numeric_limits<std::size_t>::max();

struct ReachableGraph {
    std::vector<DfaStateId> originalIds;
    std::vector<CodePointRange> alphabet;
    std::vector<std::vector<std::size_t>> transitions;
    std::vector<bool> accepting;
};

ReachableGraph collectReachable(const RegexDfa &dfa) {
    ReachableGraph graph;
    std::vector<std::size_t> localIndex(dfa.states().size(), missing);
    graph.originalIds.push_back(dfa.startState());
    localIndex[toIndex(dfa.startState())] = 0;

    for (std::size_t cursor = 0; cursor < graph.originalIds.size(); ++cursor) {
        const DfaState &state = dfa.state(graph.originalIds[cursor]);
        for (const DfaByteTransition &transition: state.byteTransitions) {
            const std::size_t target = toIndex(transition.target);
            if (localIndex[target] == missing) {
                localIndex[target] = graph.originalIds.size();
                graph.originalIds.push_back(transition.target);
            }
        }
    }

    const std::size_t dead = graph.originalIds.size();
    std::vector<CodePoint> boundaries;
    for (const DfaStateId id: graph.originalIds) {
        for (const DfaByteTransition &transition:
             dfa.state(id).byteTransitions) {
            for (const CodePointRange range: transition.bytes.ranges()) {
                boundaries.push_back(range.first);
                boundaries.push_back(range.last + 1U);
            }
        }
    }
    std::ranges::sort(boundaries);
    boundaries.erase(std::ranges::unique(boundaries).begin(),
                     boundaries.end());
    for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
        graph.alphabet.push_back(
                {boundaries[index], boundaries[index + 1] - 1U});
    }

    graph.transitions.assign(
            dead + 1, std::vector<std::size_t>(graph.alphabet.size(), dead));
    graph.accepting.reserve(dead + 1);
    for (std::size_t local = 0; local < dead; ++local) {
        auto &row = graph.transitions[local];
        const DfaState &state = dfa.state(graph.originalIds[local]);
        graph.accepting.push_back(state.accepting);
        for (const DfaByteTransition &transition: state.byteTransitions) {
            const std::size_t target = localIndex[toIndex(transition.target)];
            if (target == missing) {
                throw std::logic_error("A transition from a reachable DFA state has an unreachable target");
            }
            for (std::size_t symbol = 0; symbol < graph.alphabet.size();
                 ++symbol) {
                if (transition.bytes.contains(graph.alphabet[symbol].first)) {
                    if (row[symbol] != dead) {
                        throw std::logic_error("DFA transitions overlap");
                    }
                    row[symbol] = target;
                }
            }
        }
    }
    graph.accepting.push_back(false);
    return graph;
}

std::vector<std::size_t> refinePartitions(const ReachableGraph &graph) {
    std::vector<std::size_t> partition(graph.accepting.size());
    for (std::size_t state = 0; state < graph.accepting.size(); ++state) {
        partition[state] = graph.accepting[state] ? 1U : 0U;
    }

    while (true) {
        std::map<std::vector<std::size_t>, std::size_t> groups;
        std::vector<std::size_t> next(partition.size());
        for (std::size_t state = 0; state < partition.size(); ++state) {
            std::vector<std::size_t> signature;
            signature.reserve(graph.alphabet.size() + 1);
            signature.push_back(graph.accepting[state] ? 1U : 0U);
            for (const std::size_t target: graph.transitions[state]) {
                signature.push_back(partition[target]);
            }
            const auto [it, inserted] = groups.try_emplace(
                    std::move(signature), groups.size());
            (void) inserted;
            next[state] = it->second;
        }
        if (next == partition) return partition;
        partition = std::move(next);
    }
}

} // namespace

MinimizedRegexDfa::MinimizedRegexDfa(
        std::vector<MinimizedDfaState> states)
    : states_(std::move(states)) {
    if (states_.empty()) {
        throw std::invalid_argument("A minimized DFA must have a start state");
    }
}

DfaStateId MinimizedRegexDfa::startState() const noexcept {
    return {0};
}

std::span<const MinimizedDfaState> MinimizedRegexDfa::states() const noexcept {
    return states_;
}

const MinimizedDfaState &MinimizedRegexDfa::state(DfaStateId id) const {
    if (toIndex(id) >= states_.size()) {
        throw std::out_of_range("Unknown minimized DFA state");
    }
    return states_[toIndex(id)];
}

MinimizedRegexDfa RegexDfaMinimizer::minimize(const RegexDfa &dfa) {
    const ReachableGraph graph = collectReachable(dfa);
    const std::size_t realStateCount = graph.originalIds.size();
    const std::size_t deadState = realStateCount;
    const std::vector<std::size_t> partition = refinePartitions(graph);
    const std::size_t blockCount =
            *std::ranges::max_element(partition) + 1U;
    const std::size_t startBlock = partition[0];
    const std::size_t deadBlock = partition[deadState];

    std::vector<std::vector<std::size_t>> members(blockCount);
    for (std::size_t state = 0; state < realStateCount; ++state) {
        members[partition[state]].push_back(state);
    }

    std::vector<bool> keep(blockCount);
    for (std::size_t block = 0; block < blockCount; ++block) {
        keep[block] = !members[block].empty()
                && (block != deadBlock || block == startBlock);
    }

    std::vector<std::size_t> newId(blockCount, missing);
    std::vector<std::size_t> keptBlocks;
    const auto addBlock = [&](std::size_t block) {
        if (keep[block] && newId[block] == missing) {
            newId[block] = keptBlocks.size();
            keptBlocks.push_back(block);
        }
    };
    addBlock(startBlock);
    for (std::size_t block = 0; block < blockCount; ++block) addBlock(block);

    std::vector<MinimizedDfaState> result(keptBlocks.size());
    for (std::size_t outputId = 0; outputId < keptBlocks.size(); ++outputId) {
        const std::size_t block = keptBlocks[outputId];
        const std::size_t representative = members[block].front();
        MinimizedDfaState &output = result[outputId];
        output.accepting = graph.accepting[representative];
        for (const std::size_t local: members[block]) {
            output.originalStates.push_back(graph.originalIds[local]);
        }
        std::ranges::sort(output.originalStates);

        std::map<DfaStateId, std::vector<CodePointRange>> rangesByTarget;
        for (std::size_t symbol = 0; symbol < graph.alphabet.size();) {
            const std::size_t targetBlock =
                    partition[graph.transitions[representative][symbol]];
            CodePointRange range = graph.alphabet[symbol];
            do {
                ++symbol;
                if (symbol < graph.alphabet.size() &&
                    partition[graph.transitions[representative][symbol]] ==
                            targetBlock &&
                    range.last + 1U == graph.alphabet[symbol].first) {
                    range.last = graph.alphabet[symbol].last;
                } else {
                    break;
                }
            } while (true);
            if (!keep[targetBlock] || targetBlock == deadBlock) continue;
            rangesByTarget[DfaStateId{
                    static_cast<std::uint32_t>(newId[targetBlock])}]
                    .push_back(range);
        }
        for (auto &[target, ranges]: rangesByTarget) {
            output.byteTransitions.push_back(
                    {CodePointClass(std::move(ranges)), target});
        }
    }
    return MinimizedRegexDfa(std::move(result));
}

} // namespace zbik
