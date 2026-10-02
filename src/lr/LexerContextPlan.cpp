#include "lr/LexerContextPlan.h"

#include <sstream>
#include <utility>

namespace zbik {

LexerContextError::LexerContextError(StateId state, LookaheadWord prefix,
                                   LexerClassMask bits, std::string message)
    : std::runtime_error(std::move(message)), state_(state), prefix_(std::move(prefix)), bits_(bits) {}

LexerContextPlan::LexerContextPlan(const ParseTable &table,
                                 std::span<const LexerClassRequirement> requirements) {
    if (table.hasConflicts())
        throw std::invalid_argument("lexer context plan requires a conflict-free LR table");
    std::vector<LexerClassRequirement> byTerminal(table.grammar().terminalCount());
    for (std::size_t i = 0; i < byTerminal.size(); ++i)
        terminalNames_.push_back(table.grammar().terminalName(TerminalId{static_cast<std::uint32_t>(i)}));
    for (const auto &requirement : requirements) {
        if (toIndex(requirement.terminal) >= byTerminal.size())
            throw std::invalid_argument("lexer class requirement names an unknown terminal");
        auto &combined = byTerminal[toIndex(requirement.terminal)];
        combined.enabled |= requirement.enabled;
        combined.disabled |= requirement.disabled;
    }
    rows_.resize(table.stateCount());
    for (std::size_t state = 0; state < rows_.size(); ++state) {
        auto &nodes = rows_[state];
        nodes.emplace_back();
        for (const auto &[word, cell] : table.actionRows()[state]) {
            if (cell.empty()) continue;
            std::size_t node = 0;
            for (const auto &symbol : word.symbols()) {
                const auto child = nodes[node].children.find(symbol);
                if (child != nodes[node].children.end()) {
                    node = child->second;
                } else {
                    const auto target = nodes.size();
                    nodes[node].children.emplace(symbol, target);
                    nodes.emplace_back();
                    node = target;
                }
            }
        }
        std::vector<LookaheadSymbol> prefix;
        const auto visit = [&](const auto &self, std::size_t node) -> void {
            auto &current = nodes[node];
            for (const auto &[symbol, target] : current.children) {
                (void) target;
                if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
                    const auto &requirement = byTerminal[toIndex(*terminal)];
                    current.enabled |= requirement.enabled;
                    current.disabled |= requirement.disabled;
                }
            }
            if (const auto conflict = current.enabled & current.disabled) {
                std::ostringstream message;
                message << "incompatible lexer classes in state " << state << " after [";
                for (const auto &symbol : prefix) {
                    if (const auto terminal = std::get_if<TerminalId>(&symbol))
                        message << table.grammar().terminalName(*terminal) << ' ';
                    else message << "EOF ";
                }
                message << "]: bits 0x" << std::hex << conflict << std::dec
                        << " required both enabled and disabled; terminals:";
                for (const auto &[symbol, target] : current.children) {
                    (void) target;
                    if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
                        const auto &r = byTerminal[toIndex(*terminal)];
                        if ((r.enabled | r.disabled) & conflict)
                            message << ' ' << table.grammar().terminalName(*terminal)
                                    << "(on=0x" << std::hex << r.enabled
                                    << ",off=0x" << r.disabled << std::dec << ')';
                    }
                }
                throw LexerContextError(StateId{static_cast<std::uint32_t>(state)},
                                        LookaheadWord{prefix}, conflict, message.str());
            }
            for (const auto &[symbol, target] : current.children) {
                prefix.push_back(symbol);
                self(self, target);
                prefix.pop_back();
            }
        };
        visit(visit, 0);
    }
}

std::optional<LexerClassMask> LexerContextPlan::activeMask(
    StateId state, std::span<const LookaheadSymbol> prefix) const {
    const auto &nodes = rows_.at(toIndex(state));
    std::size_t node = 0;
    for (const auto &symbol : prefix) {
        const auto found = nodes[node].children.find(symbol);
        if (found == nodes[node].children.end()) return std::nullopt;
        node = found->second;
    }
    if (nodes[node].children.empty()) return std::nullopt;
    return nodes[node].enabled;
}

std::size_t LexerContextPlan::nodeCount() const noexcept {
    std::size_t result = 0;
    for (const auto &row : rows_) result += row.size();
    return result;
}

std::optional<TerminalId> LexerContextPlan::terminalFor(
    StateId state, std::span<const LookaheadSymbol> prefix, TerminalId source,
    std::span<const TerminalId> origins) const {
    const auto &nodes = rows_.at(toIndex(state));
    std::size_t node = 0;
    for (const auto &symbol : prefix) {
        const auto found = nodes[node].children.find(symbol);
        if (found == nodes[node].children.end()) return std::nullopt;
        node = found->second;
    }
    std::optional<TerminalId> result;
    for (const auto &[symbol, target] : nodes[node].children) {
        (void) target;
        if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
            if (origins[toIndex(*terminal)] == source) {
                if (result) throw std::logic_error("multiple scoped terminals for one lexer token");
                result = *terminal;
            }
        }
    }
    return result;
}

void LexerContextPlan::validateRules(std::span<const LexerRule> rules,
                                    std::span<const TerminalId> origins) const {
    for (std::size_t state = 0; state < rows_.size(); ++state) {
        const auto &nodes = rows_[state];
        std::vector<LookaheadSymbol> prefix;
        const auto visit = [&](const auto &self, std::size_t index) -> void {
            const auto &node = nodes[index];
            for (const auto &[symbol, target] : node.children) {
                if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
                    if (toIndex(*terminal) >= origins.size())
                        throw std::invalid_argument("incomplete scoped terminal map");
                    const auto source = origins[toIndex(*terminal)];
                    bool available = false;
                    LexerClassMask missing = 0;
                    for (const auto &rule : rules) {
                        if (rule.terminal == source) {
                            available |= lexerClassEnabled(rule.requiredClasses, node.enabled);
                            missing |= rule.requiredClasses & ~node.enabled;
                        }
                    }
                    if (!available) {
                        std::ostringstream message;
                        message << "no active lexer rule for terminal " << terminalNames_[toIndex(*terminal)]
                                << " in state " << state << " after " << LookaheadWord{prefix}.dump()
                                << "; active mask 0x" << std::hex << node.enabled
                                << ", missing class bits 0x" << missing;
                        throw LexerContextError(StateId{static_cast<std::uint32_t>(state)},
                                                LookaheadWord{prefix}, missing, message.str());
                    }
                }
                prefix.push_back(symbol);
                self(self, target);
                prefix.pop_back();
            }
        };
        visit(visit, 0);
    }
}

} // namespace zbik
