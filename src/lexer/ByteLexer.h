#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lexer/LexerAutomaton.h"
#include "lexer/LexerTypes.h"

namespace zbik {

class LexerError : public std::runtime_error {
public:
    LexerError(
            std::size_t tokenStart,
            std::size_t errorOffset,
            std::optional<std::uint8_t> byte);

    [[nodiscard]] std::size_t tokenStart() const noexcept;
    [[nodiscard]] std::size_t errorOffset() const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> byte() const noexcept;

private:
    std::size_t tokenStart_;
    std::size_t errorOffset_;
    std::optional<std::uint8_t> byte_;
};

class ByteLexer {
public:
    explicit ByteLexer(std::vector<LexerRule> rules);

    [[nodiscard]] const std::vector<LexerRule> &rules() const noexcept;
    [[nodiscard]] std::size_t dfaStateCount() const noexcept;
    [[nodiscard]] std::size_t transitionRangeCount() const noexcept;
    [[nodiscard]] const LexerAutomaton &automaton() const noexcept { return automaton_; }
    // Offset is a byte boundary. Hidden/skipped rules use the same active mask.
    [[nodiscard]] std::optional<LexedToken> next(std::string_view input, std::size_t &offset,
                                               LexerClassMask active = 0) const;
    [[nodiscard]] LexResult tokenize(std::string_view input, LexerClassMask active = 0) const;

private:
    std::vector<LexerRule> rules_;
    LexerAutomaton automaton_;
};

} // namespace zbik
