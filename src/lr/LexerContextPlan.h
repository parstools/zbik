#pragma once

#include <map>
#include <span>
#include <stdexcept>
#include <vector>

#include "lexer/LexerTypes.h"
#include "lr/ParseTable.h"

namespace zbik {

// Explicit constraints supplied by a grammar frontend. Unspecified bits are off.
// A terminal may require a class to be disabled even if its lexer rule is unconditional.
struct LexerClassRequirement {
    TerminalId terminal;
    LexerClassMask enabled{};
    LexerClassMask disabled{};
};

class LexerContextError : public std::runtime_error {
public:
    LexerContextError(StateId state, LookaheadWord prefix, LexerClassMask bits, std::string message);
    [[nodiscard]] StateId state() const noexcept { return state_; }
    [[nodiscard]] const LookaheadWord &prefix() const noexcept { return prefix_; }
    [[nodiscard]] LexerClassMask conflictingBits() const noexcept { return bits_; }
private:
    StateId state_;
    LookaheadWord prefix_;
    LexerClassMask bits_;
};

// One trie per ACTION row. No enumeration of the powerset of lexer classes.
class LexerContextPlan {
public:
    LexerContextPlan(const ParseTable &table, std::span<const LexerClassRequirement> requirements);
    [[nodiscard]] std::optional<LexerClassMask> activeMask(
        StateId state, std::span<const LookaheadSymbol> prefix) const;
    [[nodiscard]] std::size_t nodeCount() const noexcept;
    void validateRules(std::span<const LexerRule> rules, std::span<const TerminalId> origins) const;
    [[nodiscard]] std::optional<TerminalId> terminalFor(
        StateId state, std::span<const LookaheadSymbol> prefix, TerminalId source,
        std::span<const TerminalId> origins) const;
private:
    struct Node {
        std::map<LookaheadSymbol, std::size_t> children;
        LexerClassMask enabled{};
        LexerClassMask disabled{};
    };
    std::vector<std::vector<Node>> rows_;
    std::vector<std::string> terminalNames_;
};

} // namespace zbik
