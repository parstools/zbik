#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#include "LookaheadSymbols.h"

namespace zbik {

class LookaheadWord {
public:
    LookaheadWord() = default;
    explicit LookaheadWord(std::vector<LookaheadSymbol> symbols);
    LookaheadWord(std::initializer_list<LookaheadSymbol> symbols);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool endsWithEndOfInput() const noexcept;
    [[nodiscard]] std::span<const LookaheadSymbol> symbols() const noexcept;
    [[nodiscard]] std::string dump() const;

    auto operator<=>(const LookaheadWord &) const = default;
private:
    std::vector<LookaheadSymbol> symbols_;
};

struct LookaheadWordHash {
    [[nodiscard]] std::size_t operator()(const LookaheadWord &word) const noexcept;
};

} // namespace zbik

template<>
struct std::hash<zbik::LookaheadWord> {
    std::size_t operator()(const zbik::LookaheadWord &word) const noexcept {
        return zbik::LookaheadWordHash{}(word);
    }
};
