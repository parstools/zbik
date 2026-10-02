#pragma once

#include <string_view>

namespace zbik::grammar_syntax {

// Epsilon is represented by an empty right-hand side. A comment marker is
// recognized only as the first non-whitespace character of a source line.
inline constexpr char commentMarker = ';';

[[nodiscard]] bool isBlankLine(std::string_view line) noexcept;
[[nodiscard]] bool isCommentLine(std::string_view line) noexcept;

} // namespace zbik::grammar_syntax
