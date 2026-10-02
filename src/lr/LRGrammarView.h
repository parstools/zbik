#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "grammar/Grammar.h"

namespace zbik {

class LRGrammarView {
public:
    explicit LRGrammarView(const Grammar &grammar);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] std::size_t terminalCount() const noexcept;
    [[nodiscard]] std::size_t nonterminalCount() const noexcept;
    [[nodiscard]] std::size_t ruleCount() const noexcept;

    [[nodiscard]] NonterminalId start() const noexcept;
    [[nodiscard]] NonterminalId originalStart() const noexcept;
    [[nodiscard]] RuleId syntheticRuleId() const noexcept;
    [[nodiscard]] const Rule &syntheticRule() const noexcept;

    [[nodiscard]] bool isSynthetic(NonterminalId id) const noexcept;
    [[nodiscard]] bool isSynthetic(RuleId id) const noexcept;

    [[nodiscard]] const std::string &terminalName(TerminalId id) const;
    [[nodiscard]] const std::string &nonterminalName(NonterminalId id) const;
    [[nodiscard]] const std::string &symbolName(const SymbolRef &symbol) const;
    [[nodiscard]] const Rule &rule(RuleId id) const;
    [[nodiscard]] const std::vector<RuleId> &rulesFor(NonterminalId id) const;

private:
    [[nodiscard]] static std::string makeSyntheticStartName(const Grammar &grammar);

    const Grammar &grammar_;
    NonterminalId syntheticStart_;
    RuleId syntheticRuleId_;
    std::string syntheticStartName_;
    Rule syntheticRule_;
    std::vector<RuleId> syntheticRules_;
};

} // namespace zbik
