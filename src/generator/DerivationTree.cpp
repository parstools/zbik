#include "DerivationTree.h"

#include <stdexcept>
#include <utility>

namespace zbik {
namespace {

void appendQuoted(std::string &result, const std::string &text) {
    result += '"';
    for (const char character: text) {
        if (character == '"' || character == '\\') {
            result += '\\';
        }
        result += character;
    }
    result += '"';
}

} // namespace

DerivationTree::Child DerivationTree::terminal(TerminalId terminal) {
    return terminal;
}

DerivationTree::Child DerivationTree::subtree(DerivationTree tree) {
    return std::make_unique<DerivationTree>(std::move(tree));
}

DerivationTree::DerivationTree(
        const Grammar &grammar, RuleId rule, std::vector<Child> children)
    : rule_(rule), children_(std::move(children)) {
    validate(grammar, *this);
}

DerivationTree::DerivationTree(const DerivationTree &other)
    : rule_(other.rule_), children_(copyChildren(other.children_)) {}

DerivationTree &DerivationTree::operator=(const DerivationTree &other) {
    if (this != &other) {
        DerivationTree copy(other);
        swap(copy);
    }
    return *this;
}

RuleId DerivationTree::rule() const noexcept {
    return rule_;
}

NonterminalId DerivationTree::rootNonterminal(const Grammar &grammar) const {
    return grammar.rule(rule_).lhs();
}

std::span<const DerivationTree::Child> DerivationTree::children() const noexcept {
    return children_;
}

std::vector<TerminalId> DerivationTree::terminals() const {
    std::vector<TerminalId> result;
    appendTerminals(result);
    return result;
}

std::string DerivationTree::structuralFingerprint() const {
    std::string result;
    appendFingerprint(result);
    return result;
}

std::string DerivationTree::dump(const Grammar &grammar) const {
    validate(grammar, *this);
    std::string result;
    appendDump(grammar, result);
    return result;
}

bool DerivationTree::operator==(const DerivationTree &other) const {
    if (rule_ != other.rule_ || children_.size() != other.children_.size()) {
        return false;
    }
    for (std::size_t i = 0; i < children_.size(); ++i) {
        if (children_[i].index() != other.children_[i].index()) {
            return false;
        }
        if (const auto terminal = std::get_if<TerminalId>(&children_[i])) {
            if (*terminal != std::get<TerminalId>(other.children_[i])) {
                return false;
            }
        } else {
            const auto &left = std::get<std::unique_ptr<DerivationTree>>(children_[i]);
            const auto &right = std::get<std::unique_ptr<DerivationTree>>(
                    other.children_[i]);
            if (!left || !right || *left != *right) {
                return false;
            }
        }
    }
    return true;
}

void DerivationTree::swap(DerivationTree &other) noexcept {
    std::swap(rule_, other.rule_);
    children_.swap(other.children_);
}

std::vector<DerivationTree::Child> DerivationTree::copyChildren(
        std::span<const Child> children) {
    std::vector<Child> result;
    result.reserve(children.size());
    for (const Child &child: children) {
        if (const auto terminal = std::get_if<TerminalId>(&child)) {
            result.emplace_back(*terminal);
        } else {
            const auto &tree = std::get<std::unique_ptr<DerivationTree>>(child);
            result.emplace_back(tree ? std::make_unique<DerivationTree>(*tree) : nullptr);
        }
    }
    return result;
}

void DerivationTree::validate(const Grammar &grammar, const DerivationTree &tree) {
    const Rule &rule = grammar.rule(tree.rule_);
    if (rule.size() != tree.children_.size()) {
        throw std::invalid_argument("derivation children do not match rule length");
    }
    for (std::size_t i = 0; i < rule.size(); ++i) {
        const SymbolRef &symbol = rule.symbol(i);
        const Child &child = tree.children_[i];
        if (const auto expected = std::get_if<TerminalId>(&symbol)) {
            const auto actual = std::get_if<TerminalId>(&child);
            if (!actual || *actual != *expected) {
                throw std::invalid_argument("derivation terminal does not match rule");
            }
            static_cast<void>(grammar.terminalName(*actual));
            continue;
        }

        const auto childTree = std::get_if<std::unique_ptr<DerivationTree>>(&child);
        if (!childTree || !*childTree) {
            throw std::invalid_argument("derivation nonterminal requires a subtree");
        }
        validate(grammar, **childTree);
        if ((**childTree).rootNonterminal(grammar)
            != std::get<NonterminalId>(symbol)) {
            throw std::invalid_argument("derivation subtree does not match rule");
        }
    }
}

void DerivationTree::appendTerminals(std::vector<TerminalId> &result) const {
    for (const Child &child: children_) {
        if (const auto terminal = std::get_if<TerminalId>(&child)) {
            result.push_back(*terminal);
        } else {
            std::get<std::unique_ptr<DerivationTree>>(child)->appendTerminals(result);
        }
    }
}

void DerivationTree::appendFingerprint(std::string &result) const {
    result += 'R';
    result += std::to_string(rule_.value);
    result += '[';
    for (const Child &child: children_) {
        if (const auto terminal = std::get_if<TerminalId>(&child)) {
            result += 'T';
            result += std::to_string(terminal->value);
            result += ';';
        } else {
            std::get<std::unique_ptr<DerivationTree>>(child)->appendFingerprint(result);
        }
    }
    result += ']';
}

void DerivationTree::appendDump(const Grammar &grammar, std::string &result) const {
    const Rule &rule = grammar.rule(rule_);
    result += grammar.nonterminalName(rule.lhs());
    result += "#R";
    result += std::to_string(rule_.value);
    result += '(';
    for (std::size_t i = 0; i < children_.size(); ++i) {
        if (i != 0) result += ',';
        const Child &child = children_[i];
        if (const auto terminal = std::get_if<TerminalId>(&child)) {
            appendQuoted(result, grammar.terminalName(*terminal));
        } else {
            std::get<std::unique_ptr<DerivationTree>>(child)->appendDump(grammar, result);
        }
    }
    result += ')';
}

} // namespace zbik
