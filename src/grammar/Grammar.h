#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Rule.h"

namespace zbik {

class Grammar {
public:
    [[nodiscard]] std::size_t terminalCount() const noexcept;
    [[nodiscard]] std::size_t nonterminalCount() const noexcept;
    [[nodiscard]] std::size_t ruleCount() const noexcept;

    [[nodiscard]] NonterminalId start() const noexcept;
    [[nodiscard]] const std::string &terminalName(TerminalId id) const;
    [[nodiscard]] const std::string &nonterminalName(NonterminalId id) const;
    [[nodiscard]] const std::string &symbolName(const SymbolRef &symbol) const;
    [[nodiscard]] const Rule &rule(RuleId id) const;
    [[nodiscard]] const std::vector<Rule> &rules() const noexcept;
    [[nodiscard]] const std::vector<RuleId> &rulesFor(NonterminalId id) const;

    [[nodiscard]] std::optional<TerminalId> findTerminal(std::string_view name) const;
    [[nodiscard]] std::optional<NonterminalId> findNonterminal(std::string_view name) const;
    [[nodiscard]] std::string dump() const;

private:
    friend class GrammarBuilder;

    Grammar(
            std::vector<std::string> terminalNames,
            std::vector<std::string> nonterminalNames,
            std::vector<Rule> rules,
            std::vector<std::vector<RuleId>> rulesByNonterminal,
            std::unordered_map<std::string, TerminalId> terminalIds,
            std::unordered_map<std::string, NonterminalId> nonterminalIds,
            NonterminalId start);

    std::vector<std::string> terminalNames_;
    std::vector<std::string> nonterminalNames_;
    std::vector<Rule> rules_;
    std::vector<std::vector<RuleId>> rulesByNonterminal_;
    std::unordered_map<std::string, TerminalId> terminalIds_;
    std::unordered_map<std::string, NonterminalId> nonterminalIds_;
    NonterminalId start_;
};

} // namespace zbik
