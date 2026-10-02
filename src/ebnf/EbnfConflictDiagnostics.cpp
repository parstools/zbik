#include "EbnfConflictDiagnostics.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "lr/LRGrammarView.h"

namespace zbik {
namespace {

std::string repetitionName(Repetition repetition) {
    switch (repetition) {
        case Repetition::One:
            return "one";
        case Repetition::Optional:
            return "optional";
        case Repetition::ZeroOrMore:
            return "zero-or-more";
        case Repetition::OneOrMore:
            return "one-or-more";
    }
    throw std::logic_error("unknown EBNF repetition");
}

std::string originLine(const EbnfGrammarSpec &specification, const EbnfConversionResult &conversion, RuleId rule) {
    const EbnfRuleOrigin &origin = conversion.origin(rule);
    const auto rules = specification.rules();
    if (origin.sourceRuleIndex >= rules.size()) {
        throw std::invalid_argument("EBNF origin refers to an unknown source rule");
    }

    std::string result = "R" + std::to_string(rule.value) + " -> rule " + std::to_string(origin.sourceRuleIndex) +
                         " '" + rules[origin.sourceRuleIndex].lhs() + "', alternative " +
                         std::to_string(origin.alternativeIndex);
    if (origin.elementIndex) {
        result += ", element " + std::to_string(*origin.elementIndex);
        result += ", repetition " + repetitionName(origin.repetition);
    }
    if (origin.helperName) {
        result += ", helper '" + *origin.helperName + "'";
    }
    return result;
}

std::vector<RuleId> shiftRules(const LRkDfa &dfa, const Conflict &conflict) {
    if (conflict.lookahead().empty() || !std::holds_alternative<TerminalId>(conflict.lookahead().symbols().front())) {
        return {};
    }
    const TerminalId terminal = std::get<TerminalId>(conflict.lookahead().symbols().front());
    const LRGrammarView view(dfa.grammar());
    std::vector<RuleId> result;
    for (const Item &item: dfa.state(conflict.state()).items) {
        const Rule &rule = view.rule(item.rule);
        if (!view.isSynthetic(item.rule) && item.dot < rule.size() && rule.symbol(item.dot) == SymbolRef{terminal}) {
            result.push_back(item.rule);
        }
    }
    std::ranges::sort(result);
    result.erase(std::ranges::unique(result).begin(), result.end());
    return result;
}

} // namespace

std::string dumpEbnfConflict(const EbnfGrammarSpec &specification, const EbnfConversionResult &conversion,
                             const LRkDfa &dfa, const Conflict &conflict) {
    if (conversion.grammar().dump() != dfa.grammar().dump()) {
        throw std::invalid_argument("EBNF conflict diagnostics require the converted grammar used by the DFA");
    }

    std::string result = conflict.dump();
    result += "\nreductions:";
    for (const RuleId rule: conflict.reductionRules()) {
        result += "\n  " + originLine(specification, conversion, rule);
    }
    result += "\nshift items:";
    for (const RuleId rule: shiftRules(dfa, conflict)) {
        result += "\n  " + originLine(specification, conversion, rule);
    }
    result += '\n';
    return result;
}

} // namespace zbik
