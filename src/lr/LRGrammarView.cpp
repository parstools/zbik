#include "LRGrammarView.h"

#include <cstdint>

namespace zbik {

LRGrammarView::LRGrammarView(const Grammar &grammar)
    : grammar_(grammar),
      syntheticStart_{static_cast<std::uint32_t>(grammar.nonterminalCount())},
      syntheticRuleId_{static_cast<std::uint32_t>(grammar.ruleCount())},
      syntheticStartName_(makeSyntheticStartName(grammar)),
      syntheticRule_(syntheticRuleId_, syntheticStart_, {SymbolRef{grammar.start()}}),
      syntheticRules_{syntheticRuleId_} {}

const Grammar &LRGrammarView::grammar() const noexcept {
    return grammar_;
}

std::size_t LRGrammarView::terminalCount() const noexcept {
    return grammar_.terminalCount();
}

std::size_t LRGrammarView::nonterminalCount() const noexcept {
    return grammar_.nonterminalCount() + 1;
}

std::size_t LRGrammarView::ruleCount() const noexcept {
    return grammar_.ruleCount() + 1;
}

NonterminalId LRGrammarView::start() const noexcept {
    return syntheticStart_;
}

NonterminalId LRGrammarView::originalStart() const noexcept {
    return grammar_.start();
}

RuleId LRGrammarView::syntheticRuleId() const noexcept {
    return syntheticRuleId_;
}

const Rule &LRGrammarView::syntheticRule() const noexcept {
    return syntheticRule_;
}

bool LRGrammarView::isSynthetic(NonterminalId id) const noexcept {
    return id == syntheticStart_;
}

bool LRGrammarView::isSynthetic(RuleId id) const noexcept {
    return id == syntheticRuleId_;
}

const std::string &LRGrammarView::terminalName(TerminalId id) const {
    return grammar_.terminalName(id);
}

const std::string &LRGrammarView::nonterminalName(NonterminalId id) const {
    if (isSynthetic(id)) {
        return syntheticStartName_;
    }
    return grammar_.nonterminalName(id);
}

const std::string &LRGrammarView::symbolName(const SymbolRef &symbol) const {
    if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
        return terminalName(*terminal);
    }
    return nonterminalName(std::get<NonterminalId>(symbol));
}

const Rule &LRGrammarView::rule(RuleId id) const {
    if (isSynthetic(id)) {
        return syntheticRule_;
    }
    return grammar_.rule(id);
}

const std::vector<RuleId> &LRGrammarView::rulesFor(NonterminalId id) const {
    if (isSynthetic(id)) {
        return syntheticRules_;
    }
    return grammar_.rulesFor(id);
}

std::string LRGrammarView::makeSyntheticStartName(const Grammar &grammar) {
    std::string name = grammar.nonterminalName(grammar.start()) + "′";
    while (grammar.findNonterminal(name).has_value()) {
        name += "′";
    }
    return name;
}

} // namespace zbik
