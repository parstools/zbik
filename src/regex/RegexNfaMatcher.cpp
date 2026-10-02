#include "regex/RegexNfaMatcher.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace zbik {
namespace {

template<typename Input>
bool matchesBytes(const RegexNfa &nfa, const Input &input) {
    std::vector<NfaStateId> active = nfa.epsilonClosure({nfa.startState()});

    for (const auto inputByte: input) {
        const auto byte = static_cast<std::uint8_t>(static_cast<unsigned char>(inputByte));
        std::vector<NfaStateId> targets;
        for (const NfaStateId stateId: active) {
            for (const NfaByteTransition &transition: nfa.state(stateId).byteTransitions) {
                if (transition.bytes.contains(byte)) {
                    targets.push_back(transition.target);
                }
            }
        }
        if (targets.empty())
            return false;
        active = nfa.epsilonClosure(targets);
    }

    return std::ranges::binary_search(active, nfa.acceptingState());
}

struct MatcherConfig {
    NfaStateId state;
    std::size_t offset;
};

auto configIndex(MatcherConfig config, std::size_t inputSize) -> std::size_t {
    return toIndex(config.state) * (inputSize + 1) + config.offset;
}

auto preferredAfterPriority(const RegexNfa &nfa, std::span<const CodePoint> input, MatcherConfig start)
        -> std::optional<std::size_t> {
    std::unordered_set<std::size_t> visited;
    std::vector<MatcherConfig> pending{start};
    while (!pending.empty()) {
        const MatcherConfig current = pending.back();
        pending.pop_back();
        const std::size_t index = configIndex(current, input.size());
        if (!visited.insert(index).second)
            continue;
        if (current.state == nfa.acceptingState())
            return current.offset;

        const NfaState &state = nfa.state(current.state);
        for (auto transition = state.byteTransitions.rbegin(); transition != state.byteTransitions.rend();
             ++transition) {
            if (current.offset < input.size() && transition->bytes.contains(input[current.offset])) {
                pending.push_back({transition->target, current.offset + 1});
            }
        }
        for (auto target = state.epsilonTransitions.rbegin(); target != state.epsilonTransitions.rend(); ++target) {
            pending.push_back({*target, current.offset});
        }
    }
    return std::nullopt;
}

} // namespace

bool RegexNfaMatcher::matches(const RegexNfa &nfa, std::span<const std::uint8_t> input) {
    return matchesBytes(nfa, input);
}

bool RegexNfaMatcher::matches(const RegexNfa &nfa, std::string_view input) {
    return matchesBytes(nfa, input);
}

std::optional<std::size_t> RegexNfaMatcher::preferredPrefixLength(const RegexNfa &nfa,
                                                                  std::span<const CodePoint> input) {
    std::unordered_set<std::size_t> visited;
    std::vector<MatcherConfig> pending{{nfa.startState(), 0}};
    std::optional<std::size_t> result;
    while (!pending.empty()) {
        const MatcherConfig current = pending.back();
        pending.pop_back();
        const std::size_t index = configIndex(current, input.size());
        if (!visited.insert(index).second)
            continue;

        const NfaState &state = nfa.state(current.state);
        if (state.activatesPriority) {
            const auto preferred = preferredAfterPriority(nfa, input, current);
            if (preferred && (!result || *preferred > *result)) {
                result = preferred;
            }
            continue;
        }
        if (current.state == nfa.acceptingState() && (!result || current.offset > *result)) {
            result = current.offset;
        }
        for (const NfaStateId target: state.epsilonTransitions) {
            pending.push_back({target, current.offset});
        }
        if (current.offset < input.size()) {
            for (const NfaByteTransition &transition: state.byteTransitions) {
                if (transition.bytes.contains(input[current.offset])) {
                    pending.push_back({transition.target, current.offset + 1});
                }
            }
        }
    }
    return result;
}

} // namespace zbik
