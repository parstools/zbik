#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include "regex/RegexAst.h"

namespace zbik {

class RegexParseError : public std::runtime_error {
public:
    RegexParseError(std::size_t offset, std::string message);

    [[nodiscard]] std::size_t offset() const noexcept;

private:
    std::size_t offset_;
};

class RegexParser {
public:
    explicit RegexParser(CodePoint maximumCodePoint = maxUnicodeCodePoint);

    [[nodiscard]] RegexAst parse(std::string_view pattern) const;

private:
    CodePoint maximumCodePoint_;
};

} // namespace zbik
