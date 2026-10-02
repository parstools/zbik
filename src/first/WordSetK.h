#pragma once

#include <compare>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "LookaheadWord.h"

namespace zbik {

class WordSetK {
public:
    explicit WordSetK(std::size_t maxLength);
    WordSetK(std::size_t maxLength, std::vector<LookaheadWord> words);

    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::span<const LookaheadWord> words() const noexcept;
    [[nodiscard]] bool contains(const LookaheadWord &word) const;
    [[nodiscard]] std::string dump() const;

    bool add(LookaheadWord word);
    void clear() noexcept;
    bool unionWith(const WordSetK &other);

    WordSetK &operator|=(const WordSetK &other);
    friend WordSetK operator|(WordSetK lhs, const WordSetK &rhs);

    auto operator<=>(const WordSetK &) const = default;
private:
    void validate(const LookaheadWord &word) const;
    void requireCompatible(const WordSetK &other) const;

    std::size_t maxLength_;
    std::vector<LookaheadWord> words_;
};

// Computes { prefix_k(left + right) | left in lhs, right in rhs }.
// EndOfInput terminates a word before the length limit. Empty languages remain
// absorbing; this is ordinary language concatenation followed by truncation.
[[nodiscard]] WordSetK concatenateTruncated(const WordSetK &lhs, const WordSetK &rhs);

} // namespace zbik
