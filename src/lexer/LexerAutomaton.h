#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "lexer/LexerTypes.h"
#include "regex/RegexAst.h"

namespace zbik {

struct LexerAutomatonTransition {
    CodePointClass codePoints;
    std::size_t target;
};

class LexerAutomaton {
public:
    explicit LexerAutomaton(std::span<const LexerRule> rules, std::optional<CodePoint> maximumCodePoint = std::nullopt);
    explicit LexerAutomaton(std::span<const RegexAst> expressions,
                            std::optional<CodePoint> maximumCodePoint = std::nullopt,
                            std::span<const LexerClassMask> classes = {});

    [[nodiscard]] std::size_t stateCount() const noexcept;
    [[nodiscard]] std::size_t transitionRangeCount() const noexcept;
    [[nodiscard]] std::optional<std::size_t> acceptingRule(std::size_t state) const;
    [[nodiscard]] std::optional<std::size_t> acceptingRule(std::size_t state, LexerClassMask active) const;
    [[nodiscard]] bool canMatch(std::size_t state, LexerClassMask active) const;
    // Bits that can affect each rule's acceptance or extend its accepted prefix.
    [[nodiscard]] std::vector<LexerClassMask> classDependencies() const;
    [[nodiscard]] std::optional<std::size_t> nextState(std::size_t state, CodePoint codePoint) const;
    [[nodiscard]] std::span<const LexerAutomatonTransition> transitions(std::size_t state) const;
private:
    struct State {
        std::vector<std::size_t> acceptingRules;
        std::vector<std::size_t> participatingRules;
        std::vector<LexerAutomatonTransition> transitions;
    };

    std::vector<State> states_;
    std::vector<LexerClassMask> classes_;
};

} // namespace zbik
