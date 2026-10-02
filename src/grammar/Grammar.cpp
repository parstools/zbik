#include "Grammar.h"

#include <sstream>
#include <utility>

namespace zbik {

Grammar::Grammar(
        std::vector<std::string> terminalNames,
        std::vector<std::string> nonterminalNames,
        std::vector<Rule> rules,
        std::vector<std::vector<RuleId>> rulesByNonterminal,
        std::unordered_map<std::string, TerminalId> terminalIds,
        std::unordered_map<std::string, NonterminalId> nonterminalIds,
        NonterminalId start)
    : terminalNames_(std::move(terminalNames)),
      nonterminalNames_(std::move(nonterminalNames)),
      rules_(std::move(rules)),
      rulesByNonterminal_(std::move(rulesByNonterminal)),
      terminalIds_(std::move(terminalIds)),
      nonterminalIds_(std::move(nonterminalIds)),
      start_(start) {}

std::size_t Grammar::terminalCount() const noexcept {
    return terminalNames_.size();
}

std::size_t Grammar::nonterminalCount() const noexcept {
    return nonterminalNames_.size();
}

std::size_t Grammar::ruleCount() const noexcept {
    return rules_.size();
}

NonterminalId Grammar::start() const noexcept {
    return start_;
}

const std::string &Grammar::terminalName(TerminalId id) const {
    return terminalNames_.at(toIndex(id));
}

const std::string &Grammar::nonterminalName(NonterminalId id) const {
    return nonterminalNames_.at(toIndex(id));
}

const std::string &Grammar::symbolName(const SymbolRef &symbol) const {
    if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
        return terminalName(*terminal);
    }
    return nonterminalName(std::get<NonterminalId>(symbol));
}

const Rule &Grammar::rule(RuleId id) const {
    return rules_.at(toIndex(id));
}

const std::vector<Rule> &Grammar::rules() const noexcept {
    return rules_;
}

const std::vector<RuleId> &Grammar::rulesFor(NonterminalId id) const {
    return rulesByNonterminal_.at(toIndex(id));
}

std::optional<TerminalId> Grammar::findTerminal(std::string_view name) const {
    const auto found = terminalIds_.find(std::string{name});
    return found == terminalIds_.end() ? std::nullopt : std::optional{found->second};
}

std::optional<NonterminalId> Grammar::findNonterminal(std::string_view name) const {
    const auto found = nonterminalIds_.find(std::string{name});
    return found == nonterminalIds_.end() ? std::nullopt : std::optional{found->second};
}

std::string Grammar::dump() const {
    std::ostringstream output;
    output << "start: N" << start_.value << ' ' << nonterminalName(start_) << '\n';
    output << "nonterminals:\n";
    for (std::size_t i = 0; i < nonterminalNames_.size(); ++i) {
        output << "  N" << i << ": " << nonterminalNames_[i] << '\n';
    }
    output << "terminals:\n";
    for (std::size_t i = 0; i < terminalNames_.size(); ++i) {
        output << "  T" << i << ": " << terminalNames_[i] << '\n';
    }
    output << "rules:\n";
    for (const Rule &rule: rules_) {
        output << "  R" << rule.id().value << ": " << nonterminalName(rule.lhs()) << " ->";
        if (rule.rhs().empty()) {
            output << " <epsilon>";
        } else {
            for (const SymbolRef &symbol: rule.rhs()) {
                output << ' ' << symbolName(symbol);
            }
        }
        output << '\n';
    }
    return output.str();
}

} // namespace zbik
