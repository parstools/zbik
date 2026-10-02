#include "EbnfToBnf.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "grammar/GrammarBuilder.h"

namespace zbik {
namespace {

struct HelperRule {
    std::string name;
    std::string symbol;
    Repetition repetition;
    std::size_t sourceRuleIndex;
    std::size_t alternativeIndex;
    std::size_t elementIndex;
};

struct ConversionInput {
    std::vector<std::string> lines;
    std::vector<EbnfRuleOrigin> origins;
};

bool elementIsNullable(const EbnfElement &element, const std::unordered_map<std::string, bool> &nullable) {
    if (element.repetition() == Repetition::Optional || element.repetition() == Repetition::ZeroOrMore) {
        return true;
    }
    const auto found = nullable.find(element.symbol());
    return found != nullable.end() && found->second;
}

std::unordered_map<std::string, bool> computeNullable(const EbnfGrammarSpec &specification) {
    std::unordered_map<std::string, bool> nullable;
    for (const EbnfRuleSpec &rule: specification.rules()) {
        nullable.emplace(rule.lhs(), false);
    }

    bool changed;
    do {
        changed = false;
        for (const EbnfRuleSpec &rule: specification.rules()) {
            if (nullable.at(rule.lhs())) {
                continue;
            }
            for (const EbnfAlternative &alternative: rule.alternatives()) {
                bool alternativeNullable = true;
                for (const EbnfElement &element: alternative.elements()) {
                    if (!elementIsNullable(element, nullable)) {
                        alternativeNullable = false;
                        break;
                    }
                }
                if (alternativeNullable) {
                    nullable.at(rule.lhs()) = true;
                    changed = true;
                    break;
                }
            }
        }
    } while (changed);
    return nullable;
}

const char *repetitionText(Repetition repetition) {
    switch (repetition) {
        case Repetition::One:
            return "one";
        case Repetition::Optional:
            return "?";
        case Repetition::ZeroOrMore:
            return "*";
        case Repetition::OneOrMore:
            return "+";
    }
    throw std::logic_error("unknown EBNF repetition");
}

void rejectUnboundedEmptyDerivations(const EbnfGrammarSpec &specification) {
    const auto nullable = computeNullable(specification);
    const auto rules = specification.rules();
    for (std::size_t sourceRuleIndex = 0; sourceRuleIndex < rules.size(); ++sourceRuleIndex) {
        const auto alternatives = rules[sourceRuleIndex].alternatives();
        for (std::size_t alternativeIndex = 0; alternativeIndex < alternatives.size(); ++alternativeIndex) {
            const auto elements = alternatives[alternativeIndex].elements();
            for (std::size_t elementIndex = 0; elementIndex < elements.size(); ++elementIndex) {
                const EbnfElement &element = elements[elementIndex];
                if (element.repetition() != Repetition::ZeroOrMore && element.repetition() != Repetition::OneOrMore) {
                    continue;
                }
                const auto found = nullable.find(element.symbol());
                if (found != nullable.end() && found->second) {
                    throw EbnfConversionError(sourceRuleIndex, alternativeIndex, elementIndex,
                                              "repetition '" + std::string{repetitionText(element.repetition())} +
                                                      "' requires non-nullable symbol '" + element.symbol() + "'");
                }
            }
        }
    }
}

std::string ruleLine(const std::string &lhs, const std::vector<std::string> &rhs) {
    std::string result = lhs + " ->";
    for (const std::string &symbol: rhs) {
        result += ' ';
        result += symbol;
    }
    return result;
}

std::string allocateHelperName(const EbnfRuleSpec &rule, std::size_t alternativeIndex, std::size_t elementIndex,
                               std::unordered_set<std::string> &occupiedNames) {
    const std::string base =
            rule.lhs() + "__ebnf_" + std::to_string(alternativeIndex) + '_' + std::to_string(elementIndex);
    std::string candidate = base;
    for (std::size_t suffix = 2; occupiedNames.contains(candidate); ++suffix) {
        candidate = base + '_' + std::to_string(suffix);
    }
    occupiedNames.insert(candidate);
    return candidate;
}

void appendLine(ConversionInput &input, std::string line, EbnfRuleOrigin origin) {
    origin.generatedRule = RuleId{static_cast<std::uint32_t>(input.lines.size())};
    input.lines.push_back(std::move(line));
    input.origins.push_back(std::move(origin));
}

EbnfRuleOrigin helperOrigin(const HelperRule &helper, EbnfGeneratedRuleRole role) {
    return {
            RuleId{0},   helper.sourceRuleIndex, helper.alternativeIndex, helper.elementIndex, helper.repetition, role,
            helper.name,
    };
}

void appendHelperLines(ConversionInput &input, const HelperRule &helper) {
    switch (helper.repetition) {
        case Repetition::One:
            break;
        case Repetition::Optional:
            appendLine(input, ruleLine(helper.name, {helper.symbol}),
                       helperOrigin(helper, EbnfGeneratedRuleRole::OptionalPresent));
            appendLine(input, ruleLine(helper.name, {}), helperOrigin(helper, EbnfGeneratedRuleRole::OptionalEmpty));
            break;
        case Repetition::ZeroOrMore:
            appendLine(input, ruleLine(helper.name, {helper.name, helper.symbol}),
                       helperOrigin(helper, EbnfGeneratedRuleRole::RepetitionRecursive));
            appendLine(input, ruleLine(helper.name, {}), helperOrigin(helper, EbnfGeneratedRuleRole::RepetitionBase));
            break;
        case Repetition::OneOrMore:
            appendLine(input, ruleLine(helper.name, {helper.name, helper.symbol}),
                       helperOrigin(helper, EbnfGeneratedRuleRole::RepetitionRecursive));
            appendLine(input, ruleLine(helper.name, {helper.symbol}),
                       helperOrigin(helper, EbnfGeneratedRuleRole::RepetitionBase));
            break;
    }
}

ConversionInput makeConversionInput(const EbnfGrammarSpec &specification) {
    rejectUnboundedEmptyDerivations(specification);

    std::unordered_set<std::string> occupiedNames;
    for (const EbnfRuleSpec &rule: specification.rules()) {
        occupiedNames.insert(rule.lhs());
        for (const EbnfAlternative &alternative: rule.alternatives()) {
            for (const EbnfElement &element: alternative.elements()) {
                occupiedNames.insert(element.symbol());
            }
        }
    }

    ConversionInput input;
    std::vector<HelperRule> helpers;
    const auto rules = specification.rules();
    for (std::size_t sourceRuleIndex = 0; sourceRuleIndex < rules.size(); ++sourceRuleIndex) {
        const EbnfRuleSpec &rule = rules[sourceRuleIndex];
        const auto alternatives = rule.alternatives();
        for (std::size_t alternativeIndex = 0; alternativeIndex < alternatives.size(); ++alternativeIndex) {
            const auto elements = alternatives[alternativeIndex].elements();
            std::vector<std::string> rhs;
            rhs.reserve(elements.size());
            for (std::size_t elementIndex = 0; elementIndex < elements.size(); ++elementIndex) {
                const EbnfElement &element = elements[elementIndex];
                if (element.repetition() == Repetition::One) {
                    rhs.push_back(element.symbol());
                    continue;
                }

                std::string helperName = allocateHelperName(rule, alternativeIndex, elementIndex, occupiedNames);
                rhs.push_back(helperName);
                helpers.push_back({
                        std::move(helperName),
                        element.symbol(),
                        element.repetition(),
                        sourceRuleIndex,
                        alternativeIndex,
                        elementIndex,
                });
            }
            appendLine(input, ruleLine(rule.lhs(), rhs),
                       {
                               RuleId{0},
                               sourceRuleIndex,
                               alternativeIndex,
                               std::nullopt,
                               Repetition::One,
                               EbnfGeneratedRuleRole::SourceAlternative,
                               std::nullopt,
                       });
        }
    }

    for (const HelperRule &helper: helpers) {
        appendHelperLines(input, helper);
    }
    return input;
}

EbnfConversionResult buildResult(ConversionInput input, const std::vector<std::string> *declaredTerminals) {
    Grammar grammar = declaredTerminals == nullptr ? GrammarBuilder{}.build(input.lines)
                                                   : GrammarBuilder{}.build(input.lines, *declaredTerminals);
    return EbnfConversionResult{std::move(grammar), std::move(input.origins)};
}

} // namespace

EbnfConversionError::EbnfConversionError(std::size_t sourceRuleIndex, std::size_t alternativeIndex,
                                         std::size_t elementIndex, const std::string &message) :
    std::runtime_error("EBNF rule " + std::to_string(sourceRuleIndex) + ", alternative " +
                       std::to_string(alternativeIndex) + ", element " + std::to_string(elementIndex) + ": " + message),
    sourceRuleIndex_(sourceRuleIndex), alternativeIndex_(alternativeIndex), elementIndex_(elementIndex) {
}

std::size_t EbnfConversionError::sourceRuleIndex() const noexcept {
    return sourceRuleIndex_;
}

std::size_t EbnfConversionError::alternativeIndex() const noexcept {
    return alternativeIndex_;
}

std::size_t EbnfConversionError::elementIndex() const noexcept {
    return elementIndex_;
}

EbnfConversionResult::EbnfConversionResult(Grammar grammar, std::vector<EbnfRuleOrigin> origins) :
    grammar_(std::move(grammar)), origins_(std::move(origins)) {
    if (grammar_.ruleCount() != origins_.size()) {
        throw std::invalid_argument("every generated rule must have one EBNF origin");
    }
    for (std::size_t index = 0; index < origins_.size(); ++index) {
        if (origins_[index].generatedRule != RuleId{static_cast<std::uint32_t>(index)}) {
            throw std::invalid_argument("EBNF origins must follow generated RuleId order");
        }
    }
}

const Grammar &EbnfConversionResult::grammar() const noexcept {
    return grammar_;
}

std::span<const EbnfRuleOrigin> EbnfConversionResult::origins() const noexcept {
    return origins_;
}

const EbnfRuleOrigin &EbnfConversionResult::origin(RuleId rule) const {
    return origins_.at(toIndex(rule));
}

Grammar EbnfConversionResult::takeGrammar() && {
    return std::move(grammar_);
}

Grammar EbnfToBnfConverter::convert(const EbnfGrammarSpec &specification) const {
    return std::move(convertWithOrigins(specification)).takeGrammar();
}

Grammar EbnfToBnfConverter::convert(const EbnfGrammarSpec &specification,
                                    const std::vector<std::string> &declaredTerminals) const {
    return std::move(convertWithOrigins(specification, declaredTerminals)).takeGrammar();
}

EbnfConversionResult EbnfToBnfConverter::convertWithOrigins(const EbnfGrammarSpec &specification) const {
    return buildResult(makeConversionInput(specification), nullptr);
}

EbnfConversionResult EbnfToBnfConverter::convertWithOrigins(const EbnfGrammarSpec &specification,
                                                            const std::vector<std::string> &declaredTerminals) const {
    return buildResult(makeConversionInput(specification), &declaredTerminals);
}

} // namespace zbik
