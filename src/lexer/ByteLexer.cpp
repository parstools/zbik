#include "lexer/ByteLexer.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace zbik {
namespace {

std::string byteDescription(std::uint8_t byte) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << static_cast<unsigned>(byte);
    return out.str();
}

} // namespace

LexerBuildError::LexerBuildError(
        std::optional<std::size_t> ruleIndex,
        std::string message)
    : std::invalid_argument(
              ruleIndex
                      ? "lexer rule " + std::to_string(*ruleIndex) + ": " + message
                      : "lexer: " + message),
      ruleIndex_(ruleIndex) {}

std::optional<std::size_t> LexerBuildError::ruleIndex() const noexcept {
    return ruleIndex_;
}

LexerError::LexerError(
        std::size_t tokenStart,
        std::size_t errorOffset,
        std::optional<std::uint8_t> byte)
    : std::runtime_error(
              byte
                      ? "cannot form a token starting at byte "
                                + std::to_string(tokenStart)
                                + "; unexpected " + byteDescription(*byte)
                                + " at byte " + std::to_string(errorOffset)
                      : "incomplete token starting at byte "
                                + std::to_string(tokenStart)
                                + " at end of input byte "
                                + std::to_string(errorOffset)),
      tokenStart_(tokenStart), errorOffset_(errorOffset), byte_(byte) {}

std::size_t LexerError::tokenStart() const noexcept { return tokenStart_; }
std::size_t LexerError::errorOffset() const noexcept { return errorOffset_; }
std::optional<std::uint8_t> LexerError::byte() const noexcept { return byte_; }

ByteLexer::ByteLexer(std::vector<LexerRule> rules)
    : rules_(std::move(rules)), automaton_(rules_, 0xFF) {}

const std::vector<LexerRule> &ByteLexer::rules() const noexcept {
    return rules_;
}

std::size_t ByteLexer::dfaStateCount() const noexcept {
    return automaton_.stateCount();
}

std::size_t ByteLexer::transitionRangeCount() const noexcept {
    return automaton_.transitionRangeCount();
}

std::optional<LexedToken> ByteLexer::next(std::string_view input, std::size_t &offset,
                                         LexerClassMask active) const {
    if (offset > input.size()) throw std::out_of_range("lexer offset exceeds input size");
    while (offset < input.size()) {
        std::size_t state = 0;
        std::size_t cursor = offset;
        std::optional<std::size_t> matchedRule;
        std::size_t matchedEnd = offset;
        while (cursor < input.size()) {
            const auto byte = static_cast<std::uint8_t>(
                    static_cast<unsigned char>(input[cursor]));
            const std::optional<std::size_t> next =
                    automaton_.nextState(state, byte);
            if (!next || !automaton_.canMatch(*next, active)) break;
            state = *next;
            ++cursor;
            if (automaton_.acceptingRule(state, active)) {
                matchedRule = automaton_.acceptingRule(state, active);
                matchedEnd = cursor;
            }
        }
        if (!matchedRule) {
            const std::optional<std::uint8_t> byte = cursor < input.size()
                    ? std::optional<std::uint8_t>(static_cast<std::uint8_t>(
                              static_cast<unsigned char>(input[cursor])))
                    : std::nullopt;
            throw LexerError(offset, cursor, byte);
        }

        const LexerRule &rule = rules_[*matchedRule];
        if (rule.terminal) {
            const std::string text(input.substr(offset, matchedEnd - offset));
            const auto begin = offset;
            offset = matchedEnd;
            return LexedToken{*rule.terminal, begin, text};
        }
        offset = matchedEnd;
    }
    return std::nullopt;
}

LexResult ByteLexer::tokenize(std::string_view input, LexerClassMask active) const {
    LexResult result;
    std::size_t offset = 0;
    while (auto token = next(input, offset, active)) {
        result.terminalIds.push_back(token->terminal);
        result.tokens.push_back(std::move(*token));
    }
    return result;
}

} // namespace zbik
