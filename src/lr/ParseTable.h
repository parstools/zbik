#pragma once

#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Action.h"
#include "LRkDfa.h"

namespace zbik {

class LALRkDfa;
class DirectLALRkDfa;
class LRGrammarView;

using ActionRow = std::map<LookaheadWord, ActionCell>;
using GotoRow = std::map<NonterminalId, StateId>;

enum class ParseTableKind { LR, LALR, SLR };

// Builds all ACTION cells for one completed LR(k) state. Shared by the full
// table builder and the classifier that stops after the first conflict.
[[nodiscard]] ActionRow buildActionRow(
        const LRGrammarView &grammar,
        const FirstKAnalysis &first,
        const LRkState &state);

// A sparse LR parsing table. ACTION and GOTO deliberately use separate key spaces.
class ParseTable {
public:
    explicit ParseTable(const LRkDfa &dfa);
    explicit ParseTable(const LALRkDfa &dfa);
    explicit ParseTable(const DirectLALRkDfa &dfa);
    [[nodiscard]] static ParseTable slr(const Grammar &grammar, LRkDfaStats &stats);

    [[nodiscard]] const Grammar &grammar() const noexcept;
    [[nodiscard]] std::size_t maxLength() const noexcept;
    [[nodiscard]] StateId start() const noexcept;
    [[nodiscard]] std::size_t stateCount() const noexcept;
    [[nodiscard]] const ActionCell &actions(StateId state, const LookaheadWord &lookahead) const;
    [[nodiscard]] std::optional<StateId> goTo(StateId state, NonterminalId nonterminal) const;
    [[nodiscard]] std::span<const ActionRow> actionRows() const noexcept;
    [[nodiscard]] std::span<const GotoRow> gotoRows() const noexcept;
    [[nodiscard]] std::span<const Conflict> conflicts() const noexcept;
    // Full cells containing at least one action pair absent from every origin.
    // Shift targets are compared after remapping to merged state IDs.
    [[nodiscard]] std::span<const Conflict> mergeConflicts() const noexcept;
    [[nodiscard]] bool hasConflicts() const noexcept;
    [[nodiscard]] ParseTableKind kind() const noexcept;
    [[nodiscard]] bool isLalr() const noexcept;
    // Resolve only an exactly matched conflict cell; never infer a fallback.
    void resolveConflict(StateId state, const LookaheadWord &lookahead,
                         const ActionCell &expected, const Action &selected);
    [[nodiscard]] std::size_t actionTrieNodeCount(StateId state) const;
    [[nodiscard]] std::string dump() const;

private:
    explicit ParseTable(const Grammar &grammar);
    void indexActions();
    struct ActionTrieNode {
        std::map<LookaheadSymbol, std::size_t> children;
        std::optional<ActionCell> cell;
    };

    class ActionTrie {
    public:
        explicit ActionTrie(const ActionRow &row);

        [[nodiscard]] const ActionCell *find(const LookaheadWord &lookahead) const noexcept;
        [[nodiscard]] std::size_t nodeCount() const noexcept;

    private:
        std::vector<ActionTrieNode> nodes_;
    };

    [[nodiscard]] static const ActionCell &emptyCell() noexcept;

    Grammar grammar_;
    std::size_t maxLength_;
    StateId start_;
    std::vector<ActionRow> actionRows_;
    std::vector<GotoRow> gotoRows_;
    std::vector<Conflict> conflicts_;
    std::vector<Conflict> mergeConflicts_;
    std::vector<ActionTrie> actionTries_;
    ParseTableKind kind_ = ParseTableKind::LR;
};

} // namespace zbik
