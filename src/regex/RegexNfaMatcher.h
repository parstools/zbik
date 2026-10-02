#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "regex/RegexNfa.h"

namespace zbik {

class RegexNfaMatcher {
public:
    [[nodiscard]] static bool matches(const RegexNfa &nfa, std::span<const std::uint8_t> input);

    [[nodiscard]] static bool matches(const RegexNfa &nfa, std::string_view input);

    // Returns the preferred anchored prefix. Without a lazy decision this is
    // the longest accepted prefix. After a lazy decision, ordered NFA choices
    // are followed depth-first.
    [[nodiscard]] static std::optional<std::size_t> preferredPrefixLength(const RegexNfa &nfa,
                                                                          std::span<const CodePoint> input);
};

} // namespace zbik
