#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "lexer/LexerAutomaton.h"
#include "lexer/LexerTypes.h"
#include "regex/RegexNfa.h"

namespace zbik {

enum class Utf8LexerErrorKind { InvalidEncoding, NoMatchingRule };

class Utf8LexerError : public std::runtime_error {
public:
    Utf8LexerError(Utf8LexerErrorKind kind, std::size_t tokenStart, std::size_t errorOffset,
                   std::optional<CodePoint> codePoint = std::nullopt);

    [[nodiscard]] Utf8LexerErrorKind kind() const noexcept;
    [[nodiscard]] std::size_t tokenStart() const noexcept;
    [[nodiscard]] std::size_t errorOffset() const noexcept;
    [[nodiscard]] std::optional<CodePoint> codePoint() const noexcept;
private:
    Utf8LexerErrorKind kind_;
    std::size_t tokenStart_;
    std::size_t errorOffset_;
    std::optional<CodePoint> codePoint_;
};

class Utf8Lexer {
public:
    explicit Utf8Lexer(std::vector<LexerRule> rules);
    Utf8Lexer(std::vector<LexerRule> rules, std::vector<RegexAst> expressions);

    [[nodiscard]] const std::vector<LexerRule> &rules() const noexcept;
    [[nodiscard]] std::size_t dfaStateCount() const noexcept;
    [[nodiscard]] std::size_t transitionRangeCount() const noexcept;
    [[nodiscard]] const LexerAutomaton &automaton() const noexcept;
    [[nodiscard]] std::span<const RegexNfa> prioritizedNfas() const noexcept;
    // Offset is a byte boundary. Hidden/skipped rules use the same active mask.
    [[nodiscard]] std::optional<LexedToken> next(std::string_view input, std::size_t &offset,
                                               LexerClassMask active = 0) const;
    [[nodiscard]] LexResult tokenize(std::string_view input, LexerClassMask active = 0) const;
private:
    std::vector<LexerRule> rules_;
    LexerAutomaton automaton_;
    std::vector<RegexNfa> prioritizedNfas_;
};

} // namespace zbik
