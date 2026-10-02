#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "Grammar.h"

namespace zbik {

class GrammarBuildError : public std::runtime_error {
public:
    GrammarBuildError(std::size_t line, const std::string &message);

    [[nodiscard]] std::size_t line() const noexcept;

private:
    std::size_t line_;
};

class GrammarBuilder {
public:
    // The default form infers every RHS name that is not a nonterminal as a terminal.
    [[nodiscard]] Grammar build(const std::vector<std::string> &lines) const;

    // With an explicit alphabet, every RHS name must be a nonterminal or a declared terminal.
    [[nodiscard]] Grammar build(
            const std::vector<std::string> &lines,
            const std::vector<std::string> &declaredTerminals) const;

private:
    [[nodiscard]] Grammar buildImpl(
            const std::vector<std::string> &lines,
            const std::vector<std::string> *declaredTerminals) const;
};

} // namespace zbik
