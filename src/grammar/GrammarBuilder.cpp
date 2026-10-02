#include "GrammarBuilder.h"

#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "GrammarSyntax.h"
#include "util/util.h"

namespace zbik {
namespace {

struct ParsedRule {
    std::size_t line;
    std::string lhs;
    std::vector<std::string> rhs;
};

std::vector<std::string> tokenizeSymbols(const std::string &text) {
    std::istringstream input(text);
    std::vector<std::string> symbols;
    for (std::string symbol; input >> symbol;) {
        symbols.push_back(std::move(symbol));
    }
    return symbols;
}

std::vector<ParsedRule> parseRules(const std::vector<std::string> &lines) {
    std::vector<ParsedRule> parsedRules;
    parsedRules.reserve(lines.size());

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string &line = lines[index];
        const std::size_t lineNumber = index + 1;
        if (grammar_syntax::isBlankLine(line) || grammar_syntax::isCommentLine(line)) {
            continue;
        }

        const auto arrow = line.find("->");
        if (arrow == std::string::npos) {
            throw GrammarBuildError(lineNumber, "invalid rule: missing '->'");
        }
        if (line.find("->", arrow + 2) != std::string::npos) {
            throw GrammarBuildError(lineNumber, "invalid rule: more than one '->'");
        }

        ParsedRule rule;
        rule.line = lineNumber;
        rule.lhs = trim(line.substr(0, arrow));
        if (rule.lhs.empty()) {
            throw GrammarBuildError(lineNumber, "nonterminal name must not be empty");
        }
        if (tokenizeSymbols(rule.lhs).size() != 1) {
            throw GrammarBuildError(lineNumber, "nonterminal name must be a single symbol");
        }
        rule.rhs = tokenizeSymbols(line.substr(arrow + 2));
        parsedRules.push_back(std::move(rule));
    }

    if (parsedRules.empty()) {
        throw GrammarBuildError(1, "grammar has no start symbol");
    }

    return parsedRules;
}

} // namespace

GrammarBuildError::GrammarBuildError(std::size_t line, const std::string &message)
    : std::runtime_error("line " + std::to_string(line) + ": " + message), line_(line) {}

std::size_t GrammarBuildError::line() const noexcept {
    return line_;
}

Grammar GrammarBuilder::build(const std::vector<std::string> &lines) const {
    return buildImpl(lines, nullptr);
}

Grammar GrammarBuilder::build(
        const std::vector<std::string> &lines,
        const std::vector<std::string> &declaredTerminals) const {
    return buildImpl(lines, &declaredTerminals);
}

Grammar GrammarBuilder::buildImpl(
        const std::vector<std::string> &lines,
        const std::vector<std::string> *declaredTerminals) const {
    const std::vector<ParsedRule> parsedRules = parseRules(lines);
    std::vector<std::string> terminalNames;
    std::vector<std::string> nonterminalNames;
    std::vector<Rule> rules;
    std::unordered_map<std::string, TerminalId> terminalIds;
    std::unordered_map<std::string, NonterminalId> nonterminalIds;

    if (declaredTerminals != nullptr) {
        terminalNames.reserve(declaredTerminals->size());
        for (const std::string &name: *declaredTerminals) {
            if (name.empty() || tokenizeSymbols(name).size() != 1 || trim(name) != name) {
                throw GrammarBuildError(1, "terminal name must be one non-empty symbol");
            }
            const TerminalId id{static_cast<std::uint32_t>(terminalNames.size())};
            if (!terminalIds.emplace(name, id).second) {
                throw GrammarBuildError(1, "duplicate terminal declaration '" + name + "'");
            }
            terminalNames.push_back(name);
        }
    }

    for (const ParsedRule &parsedRule: parsedRules) {
        if (nonterminalIds.contains(parsedRule.lhs)) {
            continue;
        }
        if (terminalIds.contains(parsedRule.lhs)) {
            throw GrammarBuildError(
                    parsedRule.line,
                    "symbol '" + parsedRule.lhs + "' is declared as a terminal");
        }

        const NonterminalId id{static_cast<std::uint32_t>(nonterminalNames.size())};
        nonterminalIds.emplace(parsedRule.lhs, id);
        nonterminalNames.push_back(parsedRule.lhs);
    }

    std::vector<std::vector<RuleId>> rulesByNonterminal(nonterminalNames.size());
    for (const ParsedRule &parsedRule: parsedRules) {
        const NonterminalId lhsId = nonterminalIds.at(parsedRule.lhs);
        const RuleId ruleId{static_cast<std::uint32_t>(rules.size())};
        std::vector<SymbolRef> rhs;
        rhs.reserve(parsedRule.rhs.size());

        for (const std::string &symbolName: parsedRule.rhs) {
            if (const auto nonterminal = nonterminalIds.find(symbolName);
                nonterminal != nonterminalIds.end()) {
                rhs.emplace_back(nonterminal->second);
                continue;
            }

            if (declaredTerminals != nullptr) {
                const auto terminal = terminalIds.find(symbolName);
                if (terminal == terminalIds.end()) {
                    throw GrammarBuildError(
                            parsedRule.line,
                            "unknown symbol '" + symbolName + "'");
                }
                rhs.emplace_back(terminal->second);
                continue;
            }

            const auto [terminal, inserted] = terminalIds.try_emplace(
                    symbolName, TerminalId{static_cast<std::uint32_t>(terminalNames.size())});
            if (inserted) {
                terminalNames.push_back(symbolName);
            }
            rhs.emplace_back(terminal->second);
        }

        rules.emplace_back(ruleId, lhsId, std::move(rhs));
        rulesByNonterminal[toIndex(lhsId)].push_back(ruleId);
    }

    const NonterminalId start = nonterminalIds.at(parsedRules.front().lhs);
    return Grammar(
            std::move(terminalNames),
            std::move(nonterminalNames),
            std::move(rules),
            std::move(rulesByNonterminal),
            std::move(terminalIds),
            std::move(nonterminalIds),
            start);
}

} // namespace zbik
