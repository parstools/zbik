#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <limits>
#include <span>
#include <variant>

#include "grammar/Identifiers.h"

namespace zbik {

// EndOfInput is an internal parser marker and is not part of the user terminal space.
struct EndOfInput {
    auto operator<=>(const EndOfInput &) const = default;
};

inline constexpr EndOfInput endOfInput{};

using LookaheadSymbol = std::variant<TerminalId, EndOfInput>;

[[nodiscard]] constexpr bool isEndOfInput(const LookaheadSymbol &symbol) noexcept {
    return std::holds_alternative<EndOfInput>(symbol);
}

// An empty sequence represents epsilon. EOF, when present, must be the last symbol.
[[nodiscard]] bool isValidLookaheadSequence(std::span<const LookaheadSymbol> symbols) noexcept;

} // namespace zbik

template<>
struct std::hash<zbik::EndOfInput> {
    std::size_t operator()(zbik::EndOfInput) const noexcept {
        return std::numeric_limits<std::size_t>::max();
    }
};
