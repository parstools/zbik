#include "lexer/Utf8Lexer.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "regex/RegexNfaMatcher.h"

namespace zbik {
namespace {

struct DecodedCodePoint {
    CodePoint value;
    std::size_t nextOffset;
};

auto decode(std::string_view input, std::size_t offset) -> DecodedCodePoint {
    const std::size_t start = offset;
    const auto first = static_cast<unsigned char>(input[offset++]);
    if (first < 0x80)
        return {first, offset};

    unsigned length = 0;
    CodePoint value = 0;
    CodePoint minimum = 0;
    if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        value = first & 0x1FU;
        minimum = 0x80;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        value = first & 0x0FU;
        minimum = 0x800;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        value = first & 0x07U;
        minimum = 0x10000;
    } else {
        throw Utf8LexerError(Utf8LexerErrorKind::InvalidEncoding, start, start);
    }
    if (offset + length - 1 > input.size()) {
        throw Utf8LexerError(Utf8LexerErrorKind::InvalidEncoding, start, input.size());
    }
    for (unsigned index = 1; index < length; ++index) {
        const auto continuation = static_cast<unsigned char>(input[offset++]);
        if ((continuation & 0xC0U) != 0x80U) {
            throw Utf8LexerError(Utf8LexerErrorKind::InvalidEncoding, start, offset - 1);
        }
        value = (value << 6U) | (continuation & 0x3FU);
    }
    if (value < minimum || !isUnicodeScalar(value)) {
        throw Utf8LexerError(Utf8LexerErrorKind::InvalidEncoding, start, start);
    }
    return {value, offset};
}

auto codePointDescription(CodePoint codePoint) -> std::string {
    std::ostringstream out;
    out << "U+" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << codePoint;
    return out.str();
}

auto structuredAutomaton(const std::vector<LexerRule> &rules, const std::vector<RegexAst> &expressions)
        -> LexerAutomaton {
    if (rules.size() != expressions.size()) {
        throw LexerBuildError(std::nullopt, "lexer rule metadata and expression counts differ");
    }
    std::vector<LexerClassMask> classes;
    for (const auto &rule : rules) classes.push_back(rule.requiredClasses);
    return LexerAutomaton(expressions, std::nullopt, classes);
}

auto buildPrioritizedNfas(const std::vector<RegexAst> &expressions) -> std::vector<RegexNfa> {
    std::vector<RegexNfa> result;
    result.reserve(expressions.size());
    bool hasPriority = false;
    for (const RegexAst &expression: expressions) {
        result.push_back(RegexNfa::fromRegex(expression));
        hasPriority = hasPriority || result.back().hasPrioritizedDecisions();
    }
    if (!hasPriority)
        result.clear();
    return result;
}

} // namespace

Utf8LexerError::Utf8LexerError(Utf8LexerErrorKind kind, std::size_t tokenStart, std::size_t errorOffset,
                               std::optional<CodePoint> codePoint) :
    std::runtime_error(
            kind == Utf8LexerErrorKind::InvalidEncoding ? "invalid UTF-8 at byte " + std::to_string(errorOffset)
            : codePoint ? "cannot form a token starting at byte " + std::to_string(tokenStart) + "; unexpected " +
                                  codePointDescription(*codePoint) + " at byte " + std::to_string(errorOffset)
                        : "incomplete token starting at byte " + std::to_string(tokenStart) + " at end of input byte " +
                                  std::to_string(errorOffset)),
    kind_(kind), tokenStart_(tokenStart), errorOffset_(errorOffset), codePoint_(codePoint) {
}

Utf8LexerErrorKind Utf8LexerError::kind() const noexcept {
    return kind_;
}

std::size_t Utf8LexerError::tokenStart() const noexcept {
    return tokenStart_;
}

std::size_t Utf8LexerError::errorOffset() const noexcept {
    return errorOffset_;
}

std::optional<CodePoint> Utf8LexerError::codePoint() const noexcept {
    return codePoint_;
}

Utf8Lexer::Utf8Lexer(std::vector<LexerRule> rules) : rules_(std::move(rules)), automaton_(rules_) {
}

Utf8Lexer::Utf8Lexer(std::vector<LexerRule> rules, std::vector<RegexAst> expressions) :
    rules_(std::move(rules)), automaton_(structuredAutomaton(rules_, expressions)),
    prioritizedNfas_(buildPrioritizedNfas(expressions)) {
}

const std::vector<LexerRule> &Utf8Lexer::rules() const noexcept {
    return rules_;
}

std::size_t Utf8Lexer::dfaStateCount() const noexcept {
    return automaton_.stateCount();
}

std::size_t Utf8Lexer::transitionRangeCount() const noexcept {
    return automaton_.transitionRangeCount();
}

const LexerAutomaton &Utf8Lexer::automaton() const noexcept {
    return automaton_;
}

std::span<const RegexNfa> Utf8Lexer::prioritizedNfas() const noexcept {
    return prioritizedNfas_;
}

LexResult Utf8Lexer::tokenize(std::string_view input, LexerClassMask active) const {
    if (!prioritizedNfas_.empty()) {
        std::vector<CodePoint> codePoints;
        std::vector<std::size_t> byteOffsets{0};
        for (std::size_t offset = 0; offset < input.size();) {
            const DecodedCodePoint decoded = decode(input, offset);
            codePoints.push_back(decoded.value);
            offset = decoded.nextOffset;
            byteOffsets.push_back(offset);
        }

        LexResult result;
        std::size_t pointOffset = 0;
        while (pointOffset < codePoints.size()) {
            std::optional<std::size_t> matchedRule;
            std::size_t matchedLength = 0;
            const std::span<const CodePoint> remaining{codePoints.begin() + static_cast<std::ptrdiff_t>(pointOffset),
                                                       codePoints.end()};
            for (std::size_t rule = 0; rule < prioritizedNfas_.size(); ++rule) {
                if (!lexerClassEnabled(rules_[rule].requiredClasses, active)) continue;
                const auto length = RegexNfaMatcher::preferredPrefixLength(prioritizedNfas_[rule], remaining);
                if (length && *length > matchedLength) {
                    matchedRule = rule;
                    matchedLength = *length;
                }
            }
            if (!matchedRule) {
                std::size_t state = 0;
                std::size_t cursor = pointOffset;
                while (cursor < codePoints.size()) {
                    const auto next = automaton_.nextState(state, codePoints[cursor]);
                    if (!next || !automaton_.canMatch(*next, active)) {
                        throw Utf8LexerError(Utf8LexerErrorKind::NoMatchingRule, byteOffsets[pointOffset],
                                             byteOffsets[cursor], codePoints[cursor]);
                    }
                    state = *next;
                    ++cursor;
                }
                throw Utf8LexerError(Utf8LexerErrorKind::NoMatchingRule, byteOffsets[pointOffset], input.size());
            }

            const std::size_t matchedEnd = pointOffset + matchedLength;
            const LexerRule &rule = rules_[*matchedRule];
            if (rule.terminal) {
                const std::size_t byteStart = byteOffsets[pointOffset];
                const std::size_t byteEnd = byteOffsets[matchedEnd];
                const std::string text(input.substr(byteStart, byteEnd - byteStart));
                result.tokens.push_back({*rule.terminal, byteStart, text});
                result.terminalIds.push_back(*rule.terminal);
            }
            pointOffset = matchedEnd;
        }
        return result;
    }

    LexResult result;
    std::size_t offset = 0;
    while (auto token = next(input, offset, active)) {
        result.terminalIds.push_back(token->terminal);
        result.tokens.push_back(std::move(*token));
    }
    return result;
}

std::optional<LexedToken> Utf8Lexer::next(std::string_view input, std::size_t &offset,
                                         LexerClassMask active) const {
    if (offset > input.size()) throw std::out_of_range("lexer offset exceeds input size");
    if (!prioritizedNfas_.empty()) {
        std::vector<CodePoint> points;
        std::vector<std::size_t> bytes{offset};
        for (std::size_t cursor = offset; cursor < input.size();) {
            const auto decoded = decode(input, cursor);
            points.push_back(decoded.value);
            cursor = decoded.nextOffset;
            bytes.push_back(cursor);
        }
        std::size_t position = 0;
        while (position < points.size()) {
            std::optional<std::size_t> best;
            std::size_t length = 0;
            for (std::size_t rule = 0; rule < rules_.size(); ++rule) {
                if (!lexerClassEnabled(rules_[rule].requiredClasses, active)) continue;
                const auto matched = RegexNfaMatcher::preferredPrefixLength(
                    prioritizedNfas_[rule], std::span<const CodePoint>{points}.subspan(position));
                if (matched && *matched > length) { best = rule; length = *matched; }
            }
            if (!best)
                throw Utf8LexerError(Utf8LexerErrorKind::NoMatchingRule, offset, offset, points[position]);
            const auto begin = offset;
            position += length;
            offset = bytes[position];
            if (rules_[*best].terminal)
                return LexedToken{*rules_[*best].terminal, begin,
                                  std::string(input.substr(begin, offset - begin))};
        }
        return std::nullopt;
    }
    if (offset > input.size()) throw std::out_of_range("lexer offset exceeds input size");
    while (offset < input.size()) {
        std::size_t state = 0;
        std::size_t cursor = offset;
        std::optional<std::size_t> matchedRule;
        std::size_t matchedEnd = offset;
        while (cursor < input.size()) {
            DecodedCodePoint decoded;
            try {
                decoded = decode(input, cursor);
            } catch (const Utf8LexerError &error) {
                throw Utf8LexerError(Utf8LexerErrorKind::InvalidEncoding, error.errorOffset(), error.errorOffset());
            }
            const std::optional<std::size_t> next = automaton_.nextState(state, decoded.value);
            if (!next || !automaton_.canMatch(*next, active))
                break;
            state = *next;
            cursor = decoded.nextOffset;
            if (automaton_.acceptingRule(state, active)) {
                matchedRule = automaton_.acceptingRule(state, active);
                matchedEnd = cursor;
            }
        }
        if (!matchedRule) {
            if (cursor == input.size()) {
                throw Utf8LexerError(Utf8LexerErrorKind::NoMatchingRule, offset, cursor);
            }
            const DecodedCodePoint decoded = decode(input, cursor);
            throw Utf8LexerError(Utf8LexerErrorKind::NoMatchingRule, offset, cursor, decoded.value);
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

} // namespace zbik
