#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "regex/RegexNfa.h"

namespace zbik {

struct DfaStateId {
    std::uint32_t value;

    auto operator<=>(const DfaStateId &) const = default;
};

constexpr std::size_t toIndex(DfaStateId id) noexcept {
    return id.value;
}

using NfaStateSubset = std::vector<NfaStateId>;

struct DfaByteTransition {
    ByteClass bytes;
    DfaStateId target;

    friend bool operator==(const DfaByteTransition &,
                           const DfaByteTransition &) = default;
};

struct DfaState {
    NfaStateSubset nfaStates;
    bool accepting;
    std::vector<DfaByteTransition> byteTransitions;

    friend bool operator==(const DfaState &, const DfaState &) = default;
};

class RegexDfa {
public:
    // Equal subsets must produce equal hashes. Hash collisions are allowed and
    // are resolved by comparing the complete, canonical NFA state subsets.
    using StateHasher = std::function<std::size_t(const NfaStateSubset &)>;

    explicit RegexDfa(const RegexNfa &nfa, StateHasher hasher = hashSubset);

    [[nodiscard]] DfaStateId startState() const noexcept;
    [[nodiscard]] std::span<const DfaState> states() const noexcept;
    [[nodiscard]] const DfaState &state(DfaStateId id) const;

    [[nodiscard]] static std::size_t hashSubset(const NfaStateSubset &subset);

private:
    std::vector<DfaState> states_;
};

} // namespace zbik

template<>
struct std::hash<zbik::DfaStateId> {
    std::size_t operator()(zbik::DfaStateId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
