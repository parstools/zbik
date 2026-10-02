#include "LRkGoto.h"

#include <stdexcept>

namespace zbik {

LRkGoto::LRkGoto(const LRkClosure &closure) noexcept : closure_(closure) {}

const LRkClosure &LRkGoto::closure() const noexcept {
    return closure_;
}

ItemSet LRkGoto::goTo(std::span<const Item> state, const SymbolRef &symbol) const {
    validateSymbol(symbol);
    ItemSet shifted;
    for (const Item &item: state) {
        closure_.validate(item);
        const Rule &rule = closure_.grammar().rule(item.rule);
        if (item.dot == rule.size() || rule.symbol(item.dot) != symbol) {
            continue;
        }
        shifted.emplace_back(
                closure_.grammar(),
                item.rule,
                item.dot + 1,
                item.lookahead);
    }
    return closure_.close(shifted);
}

void LRkGoto::validateSymbol(const SymbolRef &symbol) const {
    const LRGrammarView &grammar = closure_.grammar();
    if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
        if (toIndex(*terminal) >= grammar.terminalCount()) {
            throw std::invalid_argument("GOTO symbol is an unknown terminal");
        }
        return;
    }
    if (toIndex(std::get<NonterminalId>(symbol)) >= grammar.nonterminalCount()) {
        throw std::invalid_argument("GOTO symbol is an unknown nonterminal");
    }
}

} // namespace zbik
