#pragma once

#include <span>

#include "LRkClosure.h"
#include "grammar/Identifiers.h"

namespace zbik {

// Computes closure({ advance(item) | item in state, symbolAfterDot(item) == symbol }).
// The closure object must outlive this object.
class LRkGoto {
public:
    explicit LRkGoto(const LRkClosure &closure) noexcept;

    [[nodiscard]] const LRkClosure &closure() const noexcept;
    [[nodiscard]] ItemSet goTo(std::span<const Item> state, const SymbolRef &symbol) const;

private:
    void validateSymbol(const SymbolRef &symbol) const;

    const LRkClosure &closure_;
};

} // namespace zbik
