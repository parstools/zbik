#pragma once

#include <cstddef>
#include <vector>

#include "FirstK.h"
#include "WordSetK.h"
#include "grammar/Grammar.h"

namespace zbik {

class FollowKAnalysis {
public:
    FollowKAnalysis(const Grammar &grammar, std::size_t maxLength);
    FollowKAnalysis(const Grammar &grammar, const FirstKAnalysis &first);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] const WordSetK &follow(NonterminalId nonterminal) const;

private:
    const Grammar &grammar_;
    std::size_t maxLength_;
    std::vector<WordSetK> follow_;
};

} // namespace zbik
