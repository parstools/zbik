#include "LRMachine.h"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <variant>

namespace zbik {
namespace {

std::string lookaheadName(const Grammar &grammar, const LookaheadSymbol &lookahead) {
    if (isEndOfInput(lookahead)) {
        return "EOF";
    }
    const TerminalId terminal = std::get<TerminalId>(lookahead);
    if (toIndex(terminal) >= grammar.terminalCount()) {
        return "terminal " + std::to_string(terminal.value);
    }
    return "'" + grammar.terminalName(terminal) + "'";
}

std::string lookaheadName(const Grammar &grammar, const LookaheadWord &lookahead) {
    if (lookahead.size() == 1) {
        return lookaheadName(grammar, lookahead.symbols().front());
    }
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < lookahead.size(); ++i) {
        if (i != 0) {
            out << ' ';
        }
        out << lookaheadName(grammar, lookahead.symbols()[i]);
    }
    out << ']';
    return out.str();
}

} // namespace

LRMachine::LRMachine(const ParseTable &table) : table_(table) {
    if (table.hasConflicts()) {
        throw std::invalid_argument(
                "LRMachine requires a conflict-free parsing table");
    }
}

LRParseResult LRMachine::parse(
        std::span<const TerminalId> input, bool captureTrace) const {
    std::vector<StateId> stack{table_.start()};
    std::vector<LRTraceStep> trace;
    std::size_t inputOffset = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (toIndex(input[i]) >= table_.grammar().terminalCount()) {
            const LookaheadWord invalid{{input[i]}};
            return {false, syntaxError(table_.start(), i, invalid), std::move(trace)};
        }
    }

    while (true) {
        const StateId state = stack.back();
        const LookaheadWord lookahead = makeLookahead(input, inputOffset);

        const ActionCell &cell = table_.actions(state, lookahead);
        if (cell.empty()) {
            return {false, syntaxError(state, inputOffset, lookahead), std::move(trace)};
        }
        if (cell.hasConflict()) {
            throw std::logic_error("conflict appeared in a validated LR parsing table");
        }

        const Action action = cell.actions().front();
        if (captureTrace) {
            trace.push_back({stack, remainingInput(input, inputOffset), lookahead, action});
        }

        if (const auto shift = std::get_if<Shift>(&action)) {
            if (lookahead.size() == 1 && isEndOfInput(lookahead.symbols().front())) {
                throw std::logic_error("LR table attempts to shift EOF");
            }
            stack.push_back(shift->target);
            ++inputOffset;
            continue;
        }

        if (const auto reduce = std::get_if<Reduce>(&action)) {
            const Rule &rule = table_.grammar().rule(reduce->rule);
            if (stack.size() <= rule.size()) {
                throw std::logic_error("LR stack underflow during reduction");
            }
            stack.resize(stack.size() - rule.size());
            const std::optional<StateId> target = table_.goTo(stack.back(), rule.lhs());
            if (!target.has_value()) {
                throw std::logic_error("missing GOTO after LR reduction");
            }
            stack.push_back(*target);
            continue;
        }

        if (inputOffset != input.size()) {
            throw std::logic_error("LR table accepts before the end of input");
        }
        return {true, std::nullopt, std::move(trace)};
    }
}

LRParseError LRMachine::syntaxError(
        StateId state,
        std::size_t inputOffset,
        const LookaheadWord &lookahead) const {
    if (lookahead.empty()) {
        throw std::logic_error("LR syntax error requires a nonempty lookahead");
    }
    std::vector<LookaheadWord> expected;
    const ActionRow &row = table_.actionRows()[toIndex(state)];
    expected.reserve(row.size());
    for (const auto &[word, cell]: row) {
        if (!cell.empty()) {
            expected.push_back(word);
        }
    }

    std::ostringstream message;
    message << "syntax error at input position " << inputOffset
            << " in state " << state.value
            << " on " << lookaheadName(table_.grammar(), lookahead);
    if (!expected.empty()) {
        message << "; expected ";
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (i != 0) {
                message << ", ";
            }
            message << lookaheadName(table_.grammar(), expected[i]);
        }
    }
    return {
            state,
            inputOffset,
            lookahead.symbols().front(),
            lookahead,
            std::move(expected),
            message.str(),
    };
}

LookaheadWord LRMachine::makeLookahead(
        std::span<const TerminalId> input,
        std::size_t inputOffset) const {
    std::vector<LookaheadSymbol> symbols;
    symbols.reserve(table_.maxLength());
    std::size_t position = inputOffset;
    while (position < input.size() && symbols.size() < table_.maxLength()) {
        symbols.emplace_back(input[position]);
        ++position;
    }
    if (position == input.size() && symbols.size() < table_.maxLength()) {
        symbols.emplace_back(endOfInput);
    }
    return LookaheadWord(std::move(symbols));
}

std::vector<LookaheadSymbol> LRMachine::remainingInput(
        std::span<const TerminalId> input,
        std::size_t inputOffset) const {
    std::vector<LookaheadSymbol> remaining;
    remaining.reserve(input.size() - inputOffset + 1);
    for (std::size_t i = inputOffset; i < input.size(); ++i) {
        remaining.emplace_back(input[i]);
    }
    remaining.emplace_back(endOfInput);
    return remaining;
}

} // namespace zbik
