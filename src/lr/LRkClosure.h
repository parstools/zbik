#pragma once

#include <span>
#include <vector>

#include "Item.h"
#include "LRGrammarView.h"
#include "first/FirstK.h"

namespace zbik {

using ItemSet = std::vector<Item>;

// Uses FIRST(k) of terminal yields. For k=0, nonproductive suffixes still
// suppress expansion; this auxiliary mode is not classical LR(0) closure.
// The grammar view and FIRST analysis must outlive this object.
class LRkClosure {
public:
    LRkClosure(const LRGrammarView &grammar, const FirstKAnalysis &first);

    [[nodiscard]] const LRGrammarView &grammar() const noexcept;
    [[nodiscard]] const FirstKAnalysis &first() const noexcept;
    void validate(const Item &item) const;
    [[nodiscard]] ItemSet close(const Item &seed) const;
    [[nodiscard]] ItemSet close(std::span<const Item> seeds) const;

private:
    void validateLookahead(const LookaheadWord &lookahead) const;

    const LRGrammarView &grammar_;
    const FirstKAnalysis &first_;
};

} // namespace zbik
