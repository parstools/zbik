#include "regex/RegexDfaMatcher.h"

#include <cstdint>
#include <optional>

namespace zbik {
namespace {

template<typename Graph, typename Input>
bool matchesBytes(const Graph &dfa, const Input &input) {
    DfaStateId current = dfa.startState();
    for (const auto inputByte: input) {
        const auto byte = static_cast<std::uint8_t>(
                static_cast<unsigned char>(inputByte));
        std::optional<DfaStateId> next;
        for (const DfaByteTransition &transition:
             dfa.state(current).byteTransitions) {
            if (transition.bytes.contains(byte)) {
                next = transition.target;
                break;
            }
        }
        if (!next) return false;
        current = *next;
    }
    return dfa.state(current).accepting;
}

} // namespace

bool RegexDfaMatcher::matches(
        const RegexDfa &dfa,
        std::span<const std::uint8_t> input) {
    return matchesBytes(dfa, input);
}

bool RegexDfaMatcher::matches(const RegexDfa &dfa, std::string_view input) {
    return matchesBytes(dfa, input);
}

bool RegexDfaMatcher::matches(
        const MinimizedRegexDfa &dfa,
        std::span<const std::uint8_t> input) {
    return matchesBytes(dfa, input);
}

bool RegexDfaMatcher::matches(
        const MinimizedRegexDfa &dfa,
        std::string_view input) {
    return matchesBytes(dfa, input);
}

} // namespace zbik
