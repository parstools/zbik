#include "SelectiveLRkMerger.h"
#include "LALRkDfa.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>

namespace zbik {
namespace {

using Core = std::vector<ItemCore>;

Core coreOf(const LRkState &state) {
    Core result;
    for (const Item &item : state.items) result.push_back(item.core());
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

Action remap(Action action, const std::vector<StateId> &map) {
    if (auto *shift = std::get_if<Shift>(&action)) {
        shift->target = map.at(toIndex(shift->target));
    }
    return action;
}

ActionRow remapRow(const ActionRow &source, const std::vector<StateId> &map) {
    ActionRow result;
    for (const auto &[word, cell] : source) {
        for (const Action &action : cell.actions()) {
            (void) result[word].add(remap(action, map));
        }
    }
    return result;
}

bool addRow(ActionRow &target, const ActionRow &source) {
    for (const auto &[word, cell] : source) {
        for (const Action &action : cell.actions()) {
            (void) target[word].add(action);
        }
        if (target[word].hasConflict()) return false;
    }
    return true;
}

struct Partition {
    std::vector<StateId> map;
    std::vector<std::vector<StateId>> members;

    explicit Partition(std::size_t size) : map(size), members(size) {
        for (std::size_t i = 0; i < size; ++i) {
            map[i] = StateId{i};
            members[i].push_back(StateId{i});
        }
    }

    void unite(std::size_t a, std::size_t b) {
        if (b < a) std::swap(a, b);
        for (StateId state : members[b]) {
            map[toIndex(state)] = StateId{a};
            members[a].push_back(state);
        }
        members[b].clear();
    }
};

bool closeTransitions(Partition &p, std::size_t a, std::size_t b,
                      const LRkDfa &canonical, const std::vector<Core> &cores) {
    std::vector<std::pair<std::size_t, std::size_t>> pending{{a, b}};
    for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const auto [left, right] = pending[cursor];
        a = toIndex(p.map[left]);
        b = toIndex(p.map[right]);
        if (a == b) continue;
        if (cores[a] != cores[b]) return false;
        const auto &x = canonical.state(StateId{a}).transitions;
        const auto &y = canonical.state(StateId{b}).transitions;
        if (x.size() != y.size()) return false;
        auto yi = y.begin();
        for (const auto &[symbol, target] : x) {
            if (symbol != yi->first) return false;
            pending.emplace_back(toIndex(target), toIndex(yi->second));
            ++yi;
        }
        // Earlier obligations remain queued even when their representatives merge.
        p.unite(a, b);
    }
    return true;
}

bool compatible(const Partition &p, const ParseTable &table,
                SelectiveMergeMode mode) {
    for (const auto &group : p.members) {
        if (group.size() < 2) continue;
        ActionRow combined = remapRow(
                table.actionRows()[toIndex(group.front())], p.map);
        for (std::size_t i = 1; i < group.size(); ++i) {
            const ActionRow next = remapRow(
                    table.actionRows()[toIndex(group[i])], p.map);
            if (mode == SelectiveMergeMode::ExactActions) {
                if (combined != next) return false;
            } else if (!addRow(combined, next)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

SelectiveMergeResult SelectiveLRkMerger::merge(
        const LRkDfa &canonical, SelectiveMergeOptions options) {
    const ParseTable table(canonical);
    if (table.hasConflicts()) {
        throw std::invalid_argument("selective merging requires conflict-free canonical LR(k)");
    }
    std::vector<Core> cores;
    for (const LRkState &state : canonical.states()) cores.push_back(coreOf(state));
    Partition partition(cores.size());
    SelectiveMergeStats stats;
    if (options.mode == SelectiveMergeMode::CompatibleUnion) {
        const LALRkDfa lalr(canonical);
        const ParseTable lalrTable(lalr);
        stats.lalrStates = lalr.states().size();
        stats.lalrConflicts = lalrTable.conflicts().size();
        if (!lalrTable.hasConflicts()) {
            for (const auto &group : lalr.originGroups()) {
                const std::size_t first = toIndex(group.front());
                for (StateId origin : group) {
                    if (toIndex(origin) != first) partition.unite(first, toIndex(origin));
                }
            }
            stats.usedLalr = true;
        }
    }
    bool changed = !stats.usedLalr;
    while (changed) {
        changed = false;
        for (std::size_t a = 0; a < cores.size() && !changed; ++a) {
            if (partition.members[a].empty()) continue;
            for (std::size_t b = a + 1; b < cores.size(); ++b) {
                if (partition.members[b].empty() || cores[a] != cores[b]) continue;
                if (stats.attempts == options.maxAttempts) {
                    stats.budgetExhausted = true;
                    break;
                }
                ++stats.attempts;
                Partition trial = partition;
                if (closeTransitions(trial, a, b, canonical, cores) &&
                    compatible(trial, table, options.mode)) {
                    partition = std::move(trial);
                    ++stats.committed;
                    changed = true;
                    break;
                }
                ++stats.rejected;
            }
            if (stats.budgetExhausted) break;
        }
        if (stats.budgetExhausted) break;
    }

    std::vector<StateId> map(cores.size());
    std::vector<std::vector<StateId>> origins;
    for (auto &group : partition.members) {
        if (group.empty()) continue;
        std::ranges::sort(group);
        for (StateId state : group) map[toIndex(state)] = StateId{origins.size()};
        origins.push_back(std::move(group));
    }
    std::vector<LRkState> states(origins.size());
    for (std::size_t i = 0; i < origins.size(); ++i) {
        std::set<Item> items;
        for (StateId origin : origins[i]) {
            const LRkState &state = canonical.state(origin);
            items.insert(state.items.begin(), state.items.end());
            for (const auto &[symbol, target] : state.transitions) {
                const StateId mapped = map[toIndex(target)];
                const auto [it, inserted] = states[i].transitions.emplace(symbol, mapped);
                if (!inserted && it->second != mapped) {
                    throw std::logic_error("selective merge has inconsistent transitions");
                }
            }
        }
        states[i].items.assign(items.begin(), items.end());
    }
    SelectiveMergeResult result{
            LRkDfa(canonical.grammar(), canonical.maxLength(), std::move(states),
                   "SelectiveLR-experimental"),
            std::move(map), std::move(origins), stats};
    if (!stats.usedLalr && result.graph.states().size() >= canonical.states().size()) {
        result.graph = canonical;
        result.statistics.retainedCanonical = true;
    }
    const std::string error = validate(canonical, result, options.mode);
    if (!error.empty()) throw std::logic_error("selective merge validation: " + error);
    return result;
}

std::string SelectiveLRkMerger::validate(
        const LRkDfa &canonical, const SelectiveMergeResult &result,
        SelectiveMergeMode mode) {
    const auto &graph = result.graph;
    const auto &map = result.canonicalToMerged;
    const auto &g = canonical.grammar();
    const auto &h = graph.grammar();
    if (g.start() != h.start() || g.ruleCount() != h.ruleCount() ||
        g.terminalCount() != h.terminalCount() ||
        g.nonterminalCount() != h.nonterminalCount()) return "grammar mismatch";
    for (const Rule &rule : g.rules()) {
        const Rule &other = h.rule(rule.id());
        if (rule.lhs() != other.lhs() || rule.rhs() != other.rhs()) return "rule mismatch";
    }
    if (graph.maxLength() != canonical.maxLength()) return "lookahead mismatch";
    if (map.size() != canonical.states().size() ||
        result.origins.size() != graph.states().size()) return "certificate size mismatch";
    if (map.at(toIndex(canonical.start())) != graph.start()) return "start mismatch";
    for (StateId target : map) {
        if (toIndex(target) >= graph.states().size()) return "unknown mapped state";
    }
    const ParseTable source(canonical);
    const ParseTable target(graph);
    if (source.hasConflicts() || target.hasConflicts()) return "conflicting table";
    std::vector<bool> seen(map.size());
    for (std::size_t i = 0; i < result.origins.size(); ++i) {
        const auto &group = result.origins[i];
        if (group.empty()) return "empty group";
        const LRkState &merged = graph.state(StateId{i});
        const Core core = coreOf(merged);
        std::set<Item> items;
        ActionRow actions;
        for (StateId origin : group) {
            const std::size_t q = toIndex(origin);
            if (q >= map.size() || seen[q]) return "duplicate or unknown origin";
            seen[q] = true;
            if (map[q] != StateId{i}) return "origin mapping mismatch";
            const LRkState &state = canonical.state(origin);
            if (coreOf(state) != core) return "core mismatch";
            items.insert(state.items.begin(), state.items.end());
            std::map<SymbolRef, StateId> transitions;
            for (const auto &[symbol, next] : state.transitions) {
                transitions.emplace(symbol, map.at(toIndex(next)));
            }
            if (transitions != merged.transitions) return "transition mismatch";
            const ActionRow row = remapRow(source.actionRows()[q], map);
            if (mode == SelectiveMergeMode::ExactActions &&
                row != target.actionRows()[i]) return "exact ACTION mismatch";
            if (!addRow(actions, row)) return "incompatible ACTION union";
        }
        if (ItemSet(items.begin(), items.end()) != merged.items) return "item union mismatch";
        if (actions != target.actionRows()[i]) return "rebuilt ACTION mismatch";
    }
    if (std::ranges::find(seen, false) != seen.end()) return "missing origin";
    return {};
}

} // namespace zbik
