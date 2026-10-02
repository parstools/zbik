#include "regex/RegexParser.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "regex/UnicodeProperties.h"

namespace zbik {
namespace {

class ParserImpl {
public:
    ParserImpl(std::string_view pattern, CodePoint maximumCodePoint)
        : pattern_(pattern), maximumCodePoint_(maximumCodePoint) {}

    RegexAst parse() {
        RegexAst result = parseAlternation();
        if (!atEnd()) fail("Unexpected ')'", position_);
        return result;
    }

private:
    RegexAst parseAlternation() {
        std::vector<RegexAst> alternatives;
        alternatives.push_back(parseConcatenation());
        while (take('|')) {
            alternatives.push_back(parseConcatenation());
        }
        return RegexAst::alternate(std::move(alternatives));
    }

    RegexAst parseConcatenation() {
        std::vector<RegexAst> elements;
        while (!atEnd() && peek() != '|' && peek() != ')') {
            elements.push_back(parseRepetition());
        }
        return RegexAst::concatenate(std::move(elements));
    }

    RegexAst parseRepetition() {
        RegexAst expression = parseAtom();
        if (atEnd()) return expression;

        RegexQuantifier quantifier;
        switch (peek()) {
            case '*': quantifier = RegexQuantifier::ZeroOrMore; break;
            case '+': quantifier = RegexQuantifier::OneOrMore; break;
            case '?': quantifier = RegexQuantifier::ZeroOrOne; break;
            default: return expression;
        }
        ++position_;
        if (!atEnd() && (peek() == '*' || peek() == '+' || peek() == '?')) {
            fail("Repeated quantifier", position_);
        }
        return RegexAst::repeat(std::move(expression), quantifier);
    }

    RegexAst parseAtom() {
        if (atEnd()) fail("Expected an expression", position_);
        const std::size_t start = position_;
        switch (peek()) {
            case '(':
                ++position_;
                {
                    RegexAst expression = parseAlternation();
                    if (!take(')')) fail("Expected ')'", position_);
                    return expression;
                }
            case '[': return parseByteClass();
            case '\'': return parseQuotedLiteral();
            case '\\':
                if (isPropertyEscape()) {
                    return RegexAst::codePointClass(parsePropertyEscape());
                }
                return singleByte(parseEscapedCodePoint());
            case '*': case '+': case '?':
                fail("Quantifier has no expression", start);
            case ')': case '|':
                fail("Expected an expression", start);
            default:
                return singleByte(readCodePoint());
        }
    }

    RegexAst parseQuotedLiteral() {
        ++position_;
        std::vector<RegexAst> codePoints;
        while (!atEnd() && peek() != '\'') {
            codePoints.push_back(singleByte(
                    peek() == '\\' ? parseEscapedCodePoint()
                                     : readCodePoint()));
        }
        if (!take('\'')) fail("Unterminated quoted literal", position_);
        return RegexAst::concatenate(std::move(codePoints));
    }

    RegexAst parseByteClass() {
        const std::size_t classStart = position_++;
        const bool negated = take('^');
        std::vector<CodePointRange> ranges;
        bool hasElement = false;
        while (!atEnd() && peek() != ']') {
            hasElement = true;
            if (isPropertyEscape()) {
                const CodePointClass property = parsePropertyEscape();
                ranges.insert(ranges.end(), property.ranges().begin(),
                              property.ranges().end());
                if (!atEnd() && peek() == '-' &&
                    position_ + 1 < pattern_.size() &&
                    pattern_[position_ + 1] != ']') {
                    fail("Unicode property cannot be a range endpoint",
                         position_);
                }
                continue;
            }
            const CodePoint first = readClassCodePoint();
            if (!atEnd() && peek() == '-'
                && position_ + 1 < pattern_.size()
                && pattern_[position_ + 1] != ']') {
                ++position_;
                const CodePoint last = readClassCodePoint();
                if (first > last) fail("Descending byte range", position_);
                ranges.push_back({first, last});
            } else {
                ranges.push_back({first, first});
            }
        }
        if (!take(']')) fail("Unterminated byte class", classStart);
        if (!hasElement) fail("Empty byte class", classStart);

        CodePointClass codePoints(std::move(ranges));
        if (negated) codePoints = complement(codePoints);
        return RegexAst::codePointClass(std::move(codePoints));
    }

    [[nodiscard]] bool isPropertyEscape() const noexcept {
        return position_ + 1 < pattern_.size() && peek() == '\\' &&
                (pattern_[position_ + 1] == 'p' ||
                 pattern_[position_ + 1] == 'P');
    }

    CodePointClass parsePropertyEscape() {
        const std::size_t slash = position_++;
        const bool negated = pattern_[position_++] == 'P';
        if (!take('{')) fail("Expected '{' after Unicode property", slash);
        const std::size_t nameStart = position_;
        while (!atEnd() && peek() != '}') ++position_;
        if (!take('}')) fail("Unterminated Unicode property", slash);
        const std::string_view name =
                pattern_.substr(nameStart, position_ - nameStart - 1);
        if (name.empty()) fail("Empty Unicode property", slash);
        try {
            CodePointClass result = restrictToAlphabet(unicodeProperty(name));
            return negated ? complement(result) : result;
        } catch (const std::invalid_argument &error) {
            fail(error.what(), slash);
        }
    }

    CodePoint readClassCodePoint() {
        if (atEnd() || peek() == ']') fail("Expected a byte", position_);
        return peek() == '\\' ? parseEscapedCodePoint() : readCodePoint();
    }

    CodePoint parseEscapedCodePoint() {
        const std::size_t slash = position_++;
        if (atEnd()) fail("Incomplete escape", slash);
        if (static_cast<unsigned char>(peek()) >= 0x80) {
            return readCodePoint();
        }
        const char escaped = pattern_[position_++];
        switch (escaped) {
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case 'f': return '\f';
            case 'v': return '\v';
            case '0': return 0;
            case 'x': return readHexEscape(slash, 2, "\\x");
            case 'u': return readHexEscape(slash, 4, "\\u");
            case 'U': return readHexEscape(slash, 8, "\\U");
            default:
                return static_cast<unsigned char>(escaped);
        }
    }

    CodePoint readHexEscape(std::size_t slash, std::size_t digits,
                            std::string_view spelling) {
        if (position_ + digits > pattern_.size()) {
            fail("Expected " + std::to_string(digits)
                         + " hexadecimal digits after " + std::string(spelling),
                 slash);
        }
        CodePoint value = 0;
        for (std::size_t index = 0; index < digits; ++index) {
            const int digit = hexValue(pattern_[position_ + index]);
            if (digit < 0) {
                fail("Expected hexadecimal digits after "
                             + std::string(spelling),
                     slash);
            }
            value = (value << 4U) | static_cast<CodePoint>(digit);
        }
        position_ += digits;
        if (!isUnicodeScalar(value)) {
            fail("Escape is not a Unicode scalar value", slash);
        }
        if (value > maximumCodePoint_) {
            fail("Escape is outside the selected alphabet", slash);
        }
        return value;
    }

    CodePoint readCodePoint() {
        const std::size_t start = position_;
        const auto first = static_cast<unsigned char>(pattern_[position_++]);
        if (first < 0x80) return first;

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
            fail("Invalid UTF-8 leading byte", start);
        }
        if (position_ + length - 1 > pattern_.size()) {
            fail("Incomplete UTF-8 sequence", start);
        }
        for (unsigned index = 1; index < length; ++index) {
            const auto continuation =
                    static_cast<unsigned char>(pattern_[position_++]);
            if ((continuation & 0xC0U) != 0x80U) {
                fail("Invalid UTF-8 continuation byte", position_ - 1);
            }
            value = (value << 6U) | (continuation & 0x3FU);
        }
        if (value < minimum || !isUnicodeScalar(value)) {
            fail("Invalid UTF-8 scalar value", start);
        }
        if (value > maximumCodePoint_) {
            fail("Code point is outside the selected alphabet", start);
        }
        return value;
    }

    static RegexAst singleByte(CodePoint codePoint) {
        return RegexAst::codePointClass(
                CodePointClass({CodePointRange{codePoint, codePoint}}));
    }

    CodePointClass restrictToAlphabet(const CodePointClass &codePoints) const {
        std::vector<CodePointRange> result;
        for (const CodePointRange range: codePoints.ranges()) {
            if (range.first > maximumCodePoint_) break;
            result.push_back(
                    {range.first, std::min(range.last, maximumCodePoint_)});
        }
        return CodePointClass(std::move(result));
    }

    CodePointClass complement(const CodePointClass &codePoints) const {
        std::vector<CodePointRange> result;
        CodePoint next = 0;
        for (const CodePointRange range: codePoints.ranges()) {
            if (next < range.first) {
                result.push_back({next, range.first - 1});
            }
            next = range.last + 1U;
        }
        if (next <= maximumCodePoint_) {
            result.push_back({next, maximumCodePoint_});
        }
        return CodePointClass(std::move(result));
    }

    static int hexValue(char value) {
        if ('0' <= value && value <= '9') return value - '0';
        if ('a' <= value && value <= 'f') return value - 'a' + 10;
        if ('A' <= value && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    [[noreturn]] static void fail(std::string message, std::size_t offset) {
        throw RegexParseError(offset, std::move(message));
    }

    [[nodiscard]] bool atEnd() const noexcept {
        return position_ == pattern_.size();
    }

    [[nodiscard]] char peek() const noexcept {
        return pattern_[position_];
    }

    bool take(char expected) noexcept {
        if (!atEnd() && peek() == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    std::string_view pattern_;
    CodePoint maximumCodePoint_;
    std::size_t position_ = 0;
};

} // namespace

RegexParseError::RegexParseError(std::size_t offset, std::string message)
    : std::runtime_error(std::move(message) + " at byte " + std::to_string(offset)),
      offset_(offset) {}

std::size_t RegexParseError::offset() const noexcept {
    return offset_;
}

RegexParser::RegexParser(CodePoint maximumCodePoint)
    : maximumCodePoint_(maximumCodePoint) {
    if (!isUnicodeScalar(maximumCodePoint)) {
        throw std::invalid_argument(
                "RegexParser alphabet maximum must be a Unicode scalar");
    }
}

RegexAst RegexParser::parse(std::string_view pattern) const {
    return ParserImpl(pattern, maximumCodePoint_).parse();
}

} // namespace zbik
