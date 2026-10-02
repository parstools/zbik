#include "ParseTable.h"

#include <sstream>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>

#include "LRGrammarView.h"
#include "DirectLALRkDfa.h"
#include "LALRkDfa.h"
#include "first/FirstK.h"
#include "first/Follow1.h"

namespace zbik {
namespace {

void addAction(ActionRow &row, const LookaheadWord &lookahead, Action action) {
    static_cast<void>(row[lookahead].add(std::move(action)));
}

std::string dumpCell(const ActionCell &cell) {
    std::ostringstream out;
    out << '{';
    for (std::size_t i = 0; i < cell.actions().size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << dumpAction(cell.actions()[i]);
    }
    out << '}';
    return out.str();
}

} // namespace

ActionRow buildActionRow(
        const LRGrammarView &grammar,
        const FirstKAnalysis &first,
        const LRkState &state) {
    if (&first.grammar() != &grammar.grammar()) {
        throw std::invalid_argument(
                "ACTION row requires FIRST(k) for the same grammar");
    }

    ActionRow result;
    for (const auto &[symbol, target]: state.transitions) {
        if (const auto terminal = std::get_if<TerminalId>(&symbol);
            terminal != nullptr && first.maxLength() == 1) {
            addAction(result, LookaheadWord{{*terminal}}, Shift{target});
        }
    }

    for (const Item &item: state.items) {
        const Rule &rule = grammar.rule(item.rule);
        if (item.dot < rule.size()) {
            if (first.maxLength() == 1) continue;
            const auto terminal = std::get_if<TerminalId>(&rule.symbol(item.dot));
            if (terminal == nullptr) continue;
            const auto transition = state.transitions.find(SymbolRef{*terminal});
            if (transition == state.transitions.end()) {
                throw std::logic_error("LR DFA item has no terminal transition");
            }
            const WordSetK lookaheads =
                    first.firstSuffix(item.rule, item.dot, item.lookahead);
            for (const LookaheadWord &lookahead: lookaheads.words()) {
                addAction(result, lookahead, Shift{transition->second});
            }
            continue;
        }
        if (grammar.isSynthetic(item.rule)) {
            if (item.lookahead != LookaheadWord{{endOfInput}}) {
                throw std::logic_error(
                        "completed synthetic item does not look ahead to EOF");
            }
            addAction(result, item.lookahead, Accept{});
        } else {
            addAction(result, item.lookahead, Reduce{item.rule});
        }
    }
    return result;
}

ParseTable::ParseTable(const LRkDfa &dfa)
    : grammar_(dfa.grammar()), maxLength_(dfa.maxLength()), start_(dfa.start()),
      actionRows_(dfa.states().size()), gotoRows_(dfa.states().size()) {
    const LRGrammarView grammar(grammar_);
    const FirstKAnalysis first(grammar_, maxLength_);
    for (std::size_t index = 0; index < dfa.states().size(); ++index) {
        const StateId state{index};
        const LRkState &dfaState = dfa.state(state);
        actionRows_[index] = buildActionRow(grammar, first, dfaState);

        for (const auto &[symbol, target]: dfaState.transitions) {
            if (std::holds_alternative<TerminalId>(symbol)) continue;

            const NonterminalId nonterminal = std::get<NonterminalId>(symbol);
            if (grammar.isSynthetic(nonterminal)) {
                throw std::logic_error("LR DFA has a transition on the synthetic start symbol");
            }
            const auto [entry, inserted] =
                    gotoRows_[index].emplace(nonterminal, target);
            if (!inserted && entry->second != target) {
                throw std::logic_error("LR DFA has inconsistent GOTO transitions");
            }
        }

    }

    indexActions();
}

void ParseTable::indexActions() {
    conflicts_.clear();
    actionTries_.clear();
    for (std::size_t index = 0; index < actionRows_.size(); ++index) {
        for (const auto &[lookahead, cell]: actionRows_[index]) {
            if (cell.hasConflict()) {
                conflicts_.emplace_back(StateId{index}, lookahead, cell);
            }
        }
        actionTries_.emplace_back(actionRows_[index]);
    }
}

void ParseTable::resolveConflict(StateId state, const LookaheadWord &lookahead,
                                 const ActionCell &expected, const Action &selected) {
    if (toIndex(state) >= actionRows_.size()) {
        throw std::out_of_range("conflict resolution state is outside the table");
    }
    auto &row = actionRows_[toIndex(state)];
    const auto entry = row.find(lookahead);
    if (entry == row.end() || !entry->second.hasConflict() ||
        entry->second != expected || !expected.contains(selected)) {
        throw std::invalid_argument("conflict resolution does not match the complete ACTION cell");
    }
    ActionCell resolved;
    static_cast<void>(resolved.add(selected));
    entry->second = std::move(resolved);
    std::erase_if(mergeConflicts_, [&](const Conflict &conflict) {
        return conflict.state() == state && conflict.lookahead() == lookahead;
    });
    indexActions();
}

ParseTable::ParseTable(const Grammar &grammar)
    : grammar_(grammar), maxLength_(1), start_{0}, kind_(ParseTableKind::SLR) {}

ParseTable ParseTable::slr(const Grammar &source, LRkDfaStats &stats) {
    ParseTable result(source);
    const LRGrammarView view(source);
    const Follow1Analysis follow(source);
    using Core = std::pair<RuleId, std::size_t>;
    using State = std::set<Core>;
    const auto close = [&](State state) {
        std::vector<Core> pending(state.begin(), state.end());
        for (std::size_t i = 0; i < pending.size(); ++i) {
            const auto [id, dot] = pending[i];
            const Rule &rule = view.rule(id);
            if (dot == rule.size()) continue;
            if (const auto nt = std::get_if<NonterminalId>(&rule.symbol(dot))) {
                for (RuleId child: view.rulesFor(*nt)) {
                    Core item{child, 0};
                    if (state.insert(item).second) pending.push_back(item);
                }
            }
        }
        return state;
    };
    std::vector<State> states;
    std::map<State, StateId> ids;
    const auto intern = [&](State state) {
        const auto [it, inserted] = ids.emplace(state, StateId{states.size()});
        if (inserted) states.push_back(std::move(state));
        return it->second;
    };
    intern(close({{view.syntheticRuleId(), 0}}));
    stats = {};
    for (std::size_t i = 0; i < states.size(); ++i) {
        ActionRow actions;
        GotoRow gotos;
        std::map<SymbolRef, State> kernels;
        stats.items += states[i].size();
        for (const auto &[id, dot]: states[i]) {
            const Rule &rule = view.rule(id);
            if (dot < rule.size()) {
                kernels[rule.symbol(dot)].emplace(id, dot + 1);
            } else if (view.isSynthetic(id)) {
                addAction(actions, LookaheadWord{{endOfInput}}, Accept{});
            } else {
                const auto set = follow.follow(rule.lhs());
                for (TerminalId terminal: set.terminals.values()) {
                    addAction(actions, LookaheadWord{{terminal}}, Reduce{id});
                }
                if (set.containsEndOfInput) {
                    addAction(actions, LookaheadWord{{endOfInput}}, Reduce{id});
                }
            }
        }
        for (auto &[symbol, kernel]: kernels) {
            const StateId target = intern(close(std::move(kernel)));
            ++stats.transitions;
            if (const auto terminal = std::get_if<TerminalId>(&symbol)) {
                addAction(actions, LookaheadWord{{*terminal}}, Shift{target});
            } else {
                gotos.emplace(std::get<NonterminalId>(symbol), target);
            }
        }
        result.actionRows_.push_back(std::move(actions));
        result.gotoRows_.push_back(std::move(gotos));
    }
    stats.states = states.size();
    result.indexActions();
    return result;
}

ParseTable::ParseTable(const LALRkDfa &dfa)
    : ParseTable(dfa.graph_) {
    kind_ = ParseTableKind::LALR;
    const ParseTable canonical(dfa.canonical());
    std::vector<StateId> targets(dfa.canonical().states().size());
    for (std::size_t i = 0; i < dfa.states().size(); ++i) {
        for (const StateId origin: dfa.canonicalOrigins(StateId{i})) {
            targets.at(toIndex(origin)) = StateId{i};
        }
    }
    for (const Conflict &conflict: conflicts_) {
        std::set<std::pair<Action, Action>> inheritedPairs;
        for (const StateId origin: dfa.canonicalOrigins(conflict.state())) {
            ActionCell remapped;
            for (Action action: canonical.actions(origin, conflict.lookahead()).actions()) {
                if (auto shift = std::get_if<Shift>(&action)) {
                    shift->target = targets.at(toIndex(shift->target));
                }
                static_cast<void>(remapped.add(action));
            }
            const auto actions = remapped.actions();
            for (std::size_t i = 0; i < actions.size(); ++i) {
                for (std::size_t j = i + 1; j < actions.size(); ++j) {
                    inheritedPairs.emplace(actions[i], actions[j]);
                }
            }
        }
        bool hasNewPair = false;
        const auto actions = conflict.actions();
        for (std::size_t i = 0; i < actions.size(); ++i) {
            for (std::size_t j = i + 1; j < actions.size(); ++j) {
                if (!inheritedPairs.contains({actions[i], actions[j]})) {
                    hasNewPair = true;
                }
            }
        }
        if (hasNewPair) {
            mergeConflicts_.push_back(conflict);
        }
    }
}

ParseTable::ParseTable(const DirectLALRkDfa &dfa)
    : ParseTable(dfa.graph_) {
    kind_ = ParseTableKind::LALR;
}

ParseTable::ActionTrie::ActionTrie(const ActionRow &row) : nodes_(1) {
    for (const auto &[lookahead, cell]: row) {
        std::size_t node = 0;
        for (const LookaheadSymbol &symbol: lookahead.symbols()) {
            const auto found = nodes_[node].children.find(symbol);
            if (found != nodes_[node].children.end()) {
                node = found->second;
                continue;
            }
            const std::size_t child = nodes_.size();
            nodes_[node].children.emplace(symbol, child);
            nodes_.emplace_back();
            node = child;
        }
        nodes_[node].cell = cell;
    }
}

const ActionCell *ParseTable::ActionTrie::find(
        const LookaheadWord &lookahead) const noexcept {
    std::size_t node = 0;
    for (const LookaheadSymbol &symbol: lookahead.symbols()) {
        const auto found = nodes_[node].children.find(symbol);
        if (found == nodes_[node].children.end()) {
            return nullptr;
        }
        node = found->second;
    }
    return nodes_[node].cell.has_value() ? &*nodes_[node].cell : nullptr;
}

std::size_t ParseTable::ActionTrie::nodeCount() const noexcept {
    return nodes_.size();
}

const Grammar &ParseTable::grammar() const noexcept {
    return grammar_;
}

std::size_t ParseTable::maxLength() const noexcept {
    return maxLength_;
}

StateId ParseTable::start() const noexcept {
    return start_;
}

std::size_t ParseTable::stateCount() const noexcept {
    return actionRows_.size();
}

const ActionCell &ParseTable::actions(
        StateId state, const LookaheadWord &lookahead) const {
    const ActionCell *cell = actionTries_.at(toIndex(state)).find(lookahead);
    return cell == nullptr ? emptyCell() : *cell;
}

std::optional<StateId> ParseTable::goTo(
        StateId state, NonterminalId nonterminal) const {
    const GotoRow &row = gotoRows_.at(toIndex(state));
    const auto found = row.find(nonterminal);
    if (found == row.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::span<const ActionRow> ParseTable::actionRows() const noexcept {
    return actionRows_;
}

std::span<const GotoRow> ParseTable::gotoRows() const noexcept {
    return gotoRows_;
}

std::span<const Conflict> ParseTable::conflicts() const noexcept {
    return conflicts_;
}

std::span<const Conflict> ParseTable::mergeConflicts() const noexcept {
    return mergeConflicts_;
}

bool ParseTable::hasConflicts() const noexcept {
    return !conflicts_.empty();
}

ParseTableKind ParseTable::kind() const noexcept {
    return kind_;
}

bool ParseTable::isLalr() const noexcept {
    return kind_ == ParseTableKind::LALR;
}

std::size_t ParseTable::actionTrieNodeCount(StateId state) const {
    return actionTries_.at(toIndex(state)).nodeCount();
}

std::string ParseTable::dump() const {
    std::ostringstream out;
    if (kind_ == ParseTableKind::SLR) {
        out << "SLR table states=";
    } else {
        out << (kind_ == ParseTableKind::LALR ? "LALR(" : "LR(")
            << maxLength_ << ") table states=";
    }
    out << stateCount()
        << " conflicts=" << conflicts_.size() << '\n';
    for (std::size_t index = 0; index < stateCount(); ++index) {
        out << "state " << index << ":\n";
        for (const auto &[lookahead, cell]: actionRows_[index]) {
            out << "  ACTION " << lookahead.dump() << " = " << dumpCell(cell) << '\n';
        }
        for (const auto &[nonterminal, target]: gotoRows_[index]) {
            out << "  GOTO N" << nonterminal.value << " = " << target.value << '\n';
        }
    }
    return out.str();
}

const ActionCell &ParseTable::emptyCell() noexcept {
    static const ActionCell empty;
    return empty;
}

} // namespace zbik
