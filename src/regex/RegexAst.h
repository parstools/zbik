#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace zbik {

using CodePoint = std::uint32_t;

inline constexpr CodePoint maxUnicodeCodePoint = 0x10FFFF;
inline constexpr CodePoint firstHighSurrogate = 0xD800;
inline constexpr CodePoint lastLowSurrogate = 0xDFFF;

[[nodiscard]] constexpr bool isUnicodeScalar(CodePoint value) noexcept {
    return value <= maxUnicodeCodePoint && !(firstHighSurrogate <= value && value <= lastLowSurrogate);
}

struct CodePointRange {
    CodePoint first;
    CodePoint last;

    friend bool operator==(const CodePointRange &, const CodePointRange &) = default;
};

class CodePointClass {
public:
    explicit CodePointClass(std::vector<CodePointRange> ranges = {});

    [[nodiscard]] const std::vector<CodePointRange> &ranges() const noexcept;
    [[nodiscard]] bool contains(CodePoint codePoint) const noexcept;
    [[nodiscard]] bool isByteClass() const noexcept;

    friend bool operator==(const CodePointClass &, const CodePointClass &) = default;
private:
    std::vector<CodePointRange> ranges_;
};

// Compatibility aliases for the existing byte-oriented API. New code should
// use the Unicode names and explicitly restrict values when building a byte
// lexer.
using ByteRange = CodePointRange;
using ByteClass = CodePointClass;

enum class RegexQuantifier {
    ZeroOrMore,
    OneOrMore,
    ZeroOrOne,
    LazyZeroOrMore,
    LazyOneOrMore,
};

class RegexAst {
public:
    enum class Kind {
        Epsilon,
        CodePointClass,
        ByteClass = CodePointClass,
        Concatenation,
        Alternation,
        Repetition,
    };

    RegexAst();
    ~RegexAst();
    RegexAst(const RegexAst &other);
    RegexAst(RegexAst &&other) noexcept;
    RegexAst &operator=(const RegexAst &other);
    RegexAst &operator=(RegexAst &&other) noexcept;

    [[nodiscard]] static RegexAst epsilon();
    [[nodiscard]] static RegexAst codePointClass(CodePointClass codePoints);
    [[nodiscard]] static RegexAst byteClass(ByteClass bytes);
    [[nodiscard]] static RegexAst concatenate(std::vector<RegexAst> elements);
    [[nodiscard]] static RegexAst alternate(std::vector<RegexAst> alternatives);
    [[nodiscard]] static RegexAst repeat(RegexAst expression, RegexQuantifier quantifier);

    [[nodiscard]] Kind kind() const noexcept;
    [[nodiscard]] const CodePointClass &codePoints() const;
    [[nodiscard]] const ByteClass &bytes() const;
    [[nodiscard]] const std::vector<RegexAst> &elements() const;
    [[nodiscard]] const std::vector<RegexAst> &alternatives() const;
    [[nodiscard]] const RegexAst &repeated() const;
    [[nodiscard]] RegexQuantifier quantifier() const;

    friend bool operator==(const RegexAst &left, const RegexAst &right);
private:
    struct Node;
    explicit RegexAst(std::unique_ptr<Node> node);

    std::unique_ptr<Node> node_;
};

} // namespace zbik
