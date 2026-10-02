#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "grammar/Identifiers.h"

namespace zbik {

using LexerClassMask = std::uint64_t;

[[nodiscard]] constexpr bool lexerClassEnabled(LexerClassMask required, LexerClassMask active) noexcept {
    return (required & active) == required;
}

struct LexerRule {
    // A missing terminal marks a skipped token, for example whitespace.
    std::optional<TerminalId> terminal;
    std::string pattern;
    // Zero is unconditional; otherwise all specified class bits are required.
    LexerClassMask requiredClasses{};
};

struct LexedToken {
    TerminalId terminal;
    std::size_t offset;
    std::string text;

    friend bool operator==(const LexedToken &, const LexedToken &) = default;
};

struct LexResult {
    std::vector<LexedToken> tokens;
    // Ready to pass directly to LRMachine::parse().
    std::vector<TerminalId> terminalIds;
};

class LexerBuildError : public std::invalid_argument {
public:
    LexerBuildError(std::optional<std::size_t> ruleIndex, std::string message);

    [[nodiscard]] std::optional<std::size_t> ruleIndex() const noexcept;

private:
    std::optional<std::size_t> ruleIndex_;
};

} // namespace zbik
