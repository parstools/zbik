#include "EbnfGrammar.h"

#include <cctype>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace zbik {
namespace {

void validateSymbolName(const std::string &name, const char *kind) {
    if (name.empty()) {
        throw std::invalid_argument(std::string{kind} + " name must not be empty");
    }
    for (const unsigned char character: name) {
        if (std::isspace(character)) {
            throw std::invalid_argument(std::string{kind} + " name must be a single symbol");
        }
    }
}

} // namespace

EbnfElement::EbnfElement(std::string symbol, Repetition repetition) :
    symbol_(std::move(symbol)), repetition_(repetition) {
    validateSymbolName(symbol_, "EBNF element");
}

const std::string &EbnfElement::symbol() const noexcept {
    return symbol_;
}

Repetition EbnfElement::repetition() const noexcept {
    return repetition_;
}

EbnfAlternative::EbnfAlternative(std::vector<EbnfElement> elements) : elements_(std::move(elements)) {
}

EbnfAlternative::EbnfAlternative(std::initializer_list<EbnfElement> elements) : elements_(elements) {
}

std::span<const EbnfElement> EbnfAlternative::elements() const noexcept {
    return elements_;
}

bool EbnfAlternative::empty() const noexcept {
    return elements_.empty();
}

EbnfRuleSpec::EbnfRuleSpec(std::string lhs, std::vector<EbnfAlternative> alternatives) :
    lhs_(std::move(lhs)), alternatives_(std::move(alternatives)) {
    validateSymbolName(lhs_, "EBNF rule");
    if (alternatives_.empty()) {
        throw std::invalid_argument("EBNF rule must contain at least one alternative");
    }
}

EbnfRuleSpec::EbnfRuleSpec(std::string lhs, std::initializer_list<EbnfAlternative> alternatives) :
    EbnfRuleSpec(std::move(lhs), std::vector<EbnfAlternative>{alternatives}) {
}

const std::string &EbnfRuleSpec::lhs() const noexcept {
    return lhs_;
}

std::span<const EbnfAlternative> EbnfRuleSpec::alternatives() const noexcept {
    return alternatives_;
}

EbnfGrammarSpec::EbnfGrammarSpec(std::vector<EbnfRuleSpec> rules) : rules_(std::move(rules)) {
    if (rules_.empty()) {
        throw std::invalid_argument("EBNF grammar must contain at least one rule");
    }

    std::unordered_set<std::string> names;
    names.reserve(rules_.size());
    for (const EbnfRuleSpec &rule: rules_) {
        if (!names.insert(rule.lhs()).second) {
            throw std::invalid_argument("duplicate EBNF rule '" + rule.lhs() + "'");
        }
    }
}

EbnfGrammarSpec::EbnfGrammarSpec(std::initializer_list<EbnfRuleSpec> rules) :
    EbnfGrammarSpec(std::vector<EbnfRuleSpec>{rules}) {
}

std::span<const EbnfRuleSpec> EbnfGrammarSpec::rules() const noexcept {
    return rules_;
}

const std::string &EbnfGrammarSpec::startSymbol() const noexcept {
    return rules_.front().lhs();
}

} // namespace zbik
