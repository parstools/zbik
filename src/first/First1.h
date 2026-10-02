#pragma once

#include <span>
#include <vector>

#include "Nullable.h"
#include "TerminalSet.h"
#include "grammar/Grammar.h"

namespace zbik {

struct First1Set {
    TerminalSet terminals;
    bool containsEmptyWord;
};

class First1Analysis {
public:
    explicit First1Analysis(const Grammar &grammar);
    First1Analysis(const Grammar &grammar, const NullableAnalysis &nullable);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] const NullableAnalysis &nullable() const noexcept;
    [[nodiscard]] First1Set first(NonterminalId nonterminal) const;
    [[nodiscard]] First1Set first(const SymbolRef &symbol) const;
    [[nodiscard]] First1Set first(std::span<const SymbolRef> symbols) const;

private:
    const Grammar &grammar_;
    NullableAnalysis nullable_;
    std::vector<TerminalSet> first_;
};

} // namespace zbik
