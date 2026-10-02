#pragma once

#include <span>
#include <vector>

#include "grammar/Grammar.h"

namespace zbik {

class NullableAnalysis {
public:
    explicit NullableAnalysis(const Grammar &grammar);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] bool isNullable(NonterminalId nonterminal) const;
    [[nodiscard]] bool isNullable(const SymbolRef &symbol) const;
    [[nodiscard]] bool isNullable(std::span<const SymbolRef> symbols) const;

private:
    const Grammar &grammar_;
    std::vector<bool> nullable_;
};

} // namespace zbik
