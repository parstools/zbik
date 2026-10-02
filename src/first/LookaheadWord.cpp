#include "LookaheadWord.h"

#include <stdexcept>
#include <utility>

namespace zbik {

LookaheadWord::LookaheadWord(std::vector<LookaheadSymbol> symbols) : symbols_(std::move(symbols)) {
    if (!isValidLookaheadSequence(symbols_)) {
        throw std::invalid_argument("EOF may occur only once and at the end of a lookahead word");
    }
}

LookaheadWord::LookaheadWord(std::initializer_list<LookaheadSymbol> symbols) :
    LookaheadWord(std::vector<LookaheadSymbol>(symbols)) {
}

std::size_t LookaheadWord::size() const noexcept {
    return symbols_.size();
}

bool LookaheadWord::empty() const noexcept {
    return symbols_.empty();
}

bool LookaheadWord::endsWithEndOfInput() const noexcept {
    return !symbols_.empty() && isEndOfInput(symbols_.back());
}

std::span<const LookaheadSymbol> LookaheadWord::symbols() const noexcept {
    return symbols_;
}

std::string LookaheadWord::dump() const {
    std::string result = "[";
    for (std::size_t i = 0; i < symbols_.size(); ++i) {
        if (i != 0) {
            result += ' ';
        }
        if (const auto terminal = std::get_if<TerminalId>(&symbols_[i])) {
            result += 't';
            result += std::to_string(terminal->value);
        } else {
            result += '$';
        }
    }
    result += ']';
    return result;
}

std::size_t LookaheadWordHash::operator()(const LookaheadWord &word) const noexcept {
    std::size_t seed = word.size();
    for (const LookaheadSymbol &symbol: word.symbols()) {
        const std::size_t value = std::hash<LookaheadSymbol>{}(symbol);
        seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    }
    return seed;
}

} // namespace zbik
