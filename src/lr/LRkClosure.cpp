#include "LRkClosure.h"

#include <set>
#include <stdexcept>
#include <utility>

namespace zbik {

LRkClosure::LRkClosure(const LRGrammarView &grammar, const FirstKAnalysis &first)
    : grammar_(grammar), first_(first) {
    if (&first.grammar() != &grammar.grammar()) {
        throw std::invalid_argument("LRkClosure requires FIRST(k) results for the same grammar");
    }
}

const LRGrammarView &LRkClosure::grammar() const noexcept {
    return grammar_;
}

const FirstKAnalysis &LRkClosure::first() const noexcept {
    return first_;
}

ItemSet LRkClosure::close(const Item &seed) const {
    return close(std::span<const Item>{&seed, 1});
}

ItemSet LRkClosure::close(std::span<const Item> seeds) const {
    std::set<Item> items;
    ItemSet pending;
    for (const Item &seed: seeds) {
        validate(seed);
        if (items.insert(seed).second) {
            pending.push_back(seed);
        }
    }

    for (std::size_t next = 0; next < pending.size(); ++next) {
        const Item item = pending[next];
        const Rule &rule = grammar_.rule(item.rule);
        if (item.dot == rule.size()) {
            continue;
        }

        const auto nonterminal = std::get_if<NonterminalId>(&rule.symbol(item.dot));
        if (nonterminal == nullptr) {
            continue;
        }

        const std::span<const SymbolRef> suffix{rule.rhs().begin() + item.dot + 1, rule.rhs().end()};
        const WordSetK lookaheads = first_.first(suffix, item.lookahead);
        for (const RuleId childRule: grammar_.rulesFor(*nonterminal)) {
            for (const LookaheadWord &lookahead: lookaheads.words()) {
                Item child{grammar_, childRule, 0, lookahead};
                if (items.insert(child).second) {
                    pending.push_back(std::move(child));
                }
            }
        }
    }

    return {items.begin(), items.end()};
}

void LRkClosure::validate(const Item &item) const {
    if (item.dot > grammar_.rule(item.rule).size()) {
        throw std::invalid_argument("item dot position exceeds rule length");
    }
    validateLookahead(item.lookahead);
}

void LRkClosure::validateLookahead(const LookaheadWord &lookahead) const {
    const std::size_t maxLength = first_.maxLength();
    if (lookahead.size() > maxLength
        || (lookahead.size() < maxLength && !lookahead.endsWithEndOfInput())) {
        throw std::invalid_argument("LR(k) item requires a full lookahead or an EOF-terminated prefix");
    }
    for (const LookaheadSymbol &symbol: lookahead.symbols()) {
        if (const auto terminal = std::get_if<TerminalId>(&symbol);
            terminal != nullptr && toIndex(*terminal) >= grammar_.terminalCount()) {
            throw std::invalid_argument("LR(k) item lookahead contains an unknown terminal");
        }
    }
}

} // namespace zbik
