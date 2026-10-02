#include "LRkDfa.h"

#include <locale>
#include <sstream>

namespace zbik {
namespace {

std::string quote(const std::string &text) {
    std::string result = "\"";
    for (const char c: text) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result + '"';
}

std::string symbolText(const LRGrammarView &view, const SymbolRef &symbol) {
    return (std::holds_alternative<TerminalId>(symbol) ? "T:" : "N:")
           + quote(view.symbolName(symbol));
}

std::string itemText(const LRGrammarView &view, const Item &item) {
    const Rule &rule = view.rule(item.rule);
    std::string result = "R" + std::to_string(item.rule.value) + ": "
                         + symbolText(view, rule.lhs()) + " ->";
    for (std::size_t i = 0; i <= rule.size(); ++i) {
        if (i == item.dot) result += " .";
        if (i < rule.size()) result += " " + symbolText(view, rule.symbol(i));
    }
    result += " , [";
    bool separator = false;
    for (const auto &symbol: item.lookahead.symbols()) {
        if (separator) result += ' ';
        separator = true;
        if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
            result += symbolText(view, *terminal);
        } else {
            result += "EOF";
        }
    }
    return result + ']';
}

} // namespace

LRkDfaStats LRkDfa::statistics() const noexcept {
    LRkDfaStats result{states_.size(), 0, 0};
    for (const auto &state: states_) {
        result.items += state.items.size();
        result.transitions += state.transitions.size();
    }
    return result;
}

std::string LRkDfa::dump() const {
    const LRGrammarView view(grammar_);
    const auto stats = statistics();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << name_ << '(' << maxLength_ << ") states=" << stats.states << " items=" << stats.items
        << " transitions=" << stats.transitions << '\n';
    for (std::size_t i = 0; i < states_.size(); ++i) {
        out << "state " << i << (i == start().value ? " (start)" : "") << ":\n";
        for (const auto &item: states_[i].items) out << "  " << itemText(view, item) << '\n';
        for (const auto &[symbol, target]: states_[i].transitions) {
            out << "  " << symbolText(view, symbol) << " -> " << target.value << '\n';
        }
    }
    return out.str();
}

std::string LRkDfa::toDot() const {
    const LRGrammarView view(grammar_);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "digraph " << name_ << "kDfa {\n  rankdir=LR;\n  node [shape=box];\n"
        << "  start [shape=point,label=\"\"];\n  start -> s" << start().value << ";\n";
    for (std::size_t i = 0; i < states_.size(); ++i) {
        std::string label = "state " + std::to_string(i);
        for (const auto &item: states_[i].items) label += "\n" + itemText(view, item);
        out << "  s" << i << " [label=" << quote(label) << "];\n";
        for (const auto &[symbol, target]: states_[i].transitions) {
            out << "  s" << i << " -> s" << target.value << " [label="
                << quote(symbolText(view, symbol)) << "];\n";
        }
    }
    out << "}\n";
    return out.str();
}

} // namespace zbik
