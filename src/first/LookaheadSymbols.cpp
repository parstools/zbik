#include "LookaheadSymbols.h"

namespace zbik {

bool isValidLookaheadSequence(std::span<const LookaheadSymbol> symbols) noexcept {
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (isEndOfInput(symbols[i]) && i + 1 != symbols.size()) {
            return false;
        }
    }
    return true;
}

} // namespace zbik
