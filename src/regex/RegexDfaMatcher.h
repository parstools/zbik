#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "regex/RegexDfaMinimizer.h"

namespace zbik {

class RegexDfaMatcher {
public:
    [[nodiscard]] static bool matches(
            const RegexDfa &dfa,
            std::span<const std::uint8_t> input);
    [[nodiscard]] static bool matches(
            const RegexDfa &dfa,
            std::string_view input);

    [[nodiscard]] static bool matches(
            const MinimizedRegexDfa &dfa,
            std::span<const std::uint8_t> input);
    [[nodiscard]] static bool matches(
            const MinimizedRegexDfa &dfa,
            std::string_view input);
};

} // namespace zbik
