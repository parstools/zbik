#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ParseTable.h"

namespace zbik {

struct LRTraceStep {
    std::vector<StateId> stack;
    std::vector<LookaheadSymbol> remainingInput;
    LookaheadWord lookahead;
    Action action;

    bool operator==(const LRTraceStep &) const = default;
};

struct LRParseError {
    StateId state;
    std::size_t inputOffset;
    LookaheadSymbol lookahead;
    LookaheadWord lookaheadWord;
    std::vector<LookaheadWord> expected;
    std::string message;

    bool operator==(const LRParseError &) const = default;
};

struct LRParseResult {
    bool accepted;
    std::optional<LRParseError> error;
    std::vector<LRTraceStep> trace;
};

// Deterministic execution of a conflict-free canonical LR(k) table.
class LRMachine {
public:
    explicit LRMachine(const ParseTable &table);
    LRMachine(ParseTable &&) = delete;

    [[nodiscard]] LRParseResult parse(
            std::span<const TerminalId> input,
            bool captureTrace = false) const;

private:
    [[nodiscard]] LRParseError syntaxError(
            StateId state,
            std::size_t inputOffset,
            const LookaheadWord &lookahead) const;
    [[nodiscard]] LookaheadWord makeLookahead(
            std::span<const TerminalId> input,
            std::size_t inputOffset) const;
    [[nodiscard]] std::vector<LookaheadSymbol> remainingInput(
            std::span<const TerminalId> input,
            std::size_t inputOffset) const;

    const ParseTable &table_;
};

} // namespace zbik
