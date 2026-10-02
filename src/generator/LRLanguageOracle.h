#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "grammar/Grammar.h"
#include "lr/ParseTable.h"

namespace zbik {

struct LRLanguageMismatch {
    std::vector<TerminalId> word;
    bool generated;
    bool accepted;
    std::string parserError;

    bool operator==(const LRLanguageMismatch &) const = default;
};

struct LRLanguageOracleResult {
    std::size_t generatedTrees;
    std::size_t generatedWords;
    std::size_t testedWords;
    std::vector<LRLanguageMismatch> mismatches;

    [[nodiscard]] bool matches() const noexcept;
};

[[nodiscard]] LRLanguageOracleResult compareGeneratedLanguage(
        const Grammar &grammar, const ParseTable &table,
        std::size_t maxWordLength);

} // namespace zbik
