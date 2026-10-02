#pragma once

#include <vector>

#include "First1.h"
#include "TerminalSet.h"
#include "grammar/Grammar.h"

namespace zbik {

struct Follow1Set {
    TerminalSet terminals;
    bool containsEndOfInput = false;

    bool operator==(const Follow1Set &) const = default;
};

class Follow1Analysis {
public:
    explicit Follow1Analysis(const Grammar &grammar);
    Follow1Analysis(const Grammar &grammar, const First1Analysis &first);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] Follow1Set follow(NonterminalId nonterminal) const;
private:
    const Grammar &grammar_;
    std::vector<Follow1Set> follow_;
};

} // namespace zbik
