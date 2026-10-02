#pragma once

#include "ParseTable.h"

namespace zbik {

struct TableStorageStats {
    std::size_t uncompressedBytes = 0;
    std::size_t compressedBytes = 0;
    std::size_t uniqueActionRows = 0;
    std::size_t uniqueGotoRows = 0;
};

// Executable default-reduction representation. Missing ACTION keys may reduce
// instead of reporting an immediate error; original non-error actions are exact.
// Estimates describe a packed format, not the heap usage of these containers.
class CompressedParseTable {
public:
    explicit CompressedParseTable(const ParseTable &table);
    [[nodiscard]] std::optional<Action> action(StateId state, const LookaheadWord &word) const;
    [[nodiscard]] std::optional<StateId> goTo(StateId state, NonterminalId symbol) const;
    [[nodiscard]] const TableStorageStats &statistics() const noexcept { return stats_; }
    // Deterministic, language-neutral text. The caller decides whether to
    // display it, write it to a file, or embed it in another artifact.
    [[nodiscard]] std::string dumpJson() const;
    [[nodiscard]] std::string dumpDsl() const;

private:
    struct Row {
        std::optional<Action> fallback;
        std::map<LookaheadWord, Action> exceptions;
        bool operator==(const Row &) const = default;
    };
    ParseTableKind kind_;
    StateId start_;
    std::size_t k_;
    std::size_t terminals_;
    std::size_t nonterminals_;
    std::vector<std::string> terminalNames_;
    std::vector<std::string> nonterminalNames_;
    std::vector<Row> rows_;
    std::vector<GotoRow> gotos_;
    std::vector<std::size_t> rowIds_, gotoIds_;
    TableStorageStats stats_;
};

} // namespace zbik
