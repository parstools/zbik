#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <span>
#include <vector>

#include "regex/RegexAst.h"

namespace zbik {

struct NfaStateId {
    std::uint32_t value;

    auto operator<=>(const NfaStateId &) const = default;
};

constexpr std::size_t toIndex(NfaStateId id) noexcept {
    return id.value;
}

struct NfaByteTransition {
    ByteClass bytes;
    NfaStateId target;

    friend bool operator==(const NfaByteTransition &, const NfaByteTransition &) = default;
};

struct NfaState {
    std::vector<NfaStateId> epsilonTransitions;
    std::vector<NfaByteTransition> byteTransitions;
    // Ordered decisions are interpreted only by the prioritized matcher.
    bool orderedDecision{};
    bool activatesPriority{};

    friend bool operator==(const NfaState &, const NfaState &) = default;
};

class RegexNfa {
public:
    RegexNfa(std::vector<NfaState> states, NfaStateId startState, NfaStateId acceptingState);

    [[nodiscard]] static RegexNfa fromRegex(const RegexAst &expression);

    [[nodiscard]] NfaStateId startState() const noexcept;
    [[nodiscard]] NfaStateId acceptingState() const noexcept;
    [[nodiscard]] std::span<const NfaState> states() const noexcept;
    [[nodiscard]] const NfaState &state(NfaStateId id) const;
    [[nodiscard]] bool hasPrioritizedDecisions() const noexcept;

    [[nodiscard]] std::vector<NfaStateId> epsilonClosure(std::span<const NfaStateId> seeds) const;
    [[nodiscard]] std::vector<NfaStateId> epsilonClosure(std::initializer_list<NfaStateId> seeds) const;
private:
    std::vector<NfaState> states_;
    NfaStateId startState_;
    NfaStateId acceptingState_;
    bool hasPrioritizedDecisions_{};
};

} // namespace zbik

template<>
struct std::hash<zbik::NfaStateId> {
    std::size_t operator()(zbik::NfaStateId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
