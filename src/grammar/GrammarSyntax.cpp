#include "GrammarSyntax.h"

namespace zbik::grammar_syntax {

bool isBlankLine(std::string_view line) noexcept {
    return line.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

bool isCommentLine(std::string_view line) noexcept {
    const std::size_t first = line.find_first_not_of(" \t");
    return first != std::string_view::npos && line[first] == commentMarker;
}

} // namespace zbik::grammar_syntax
