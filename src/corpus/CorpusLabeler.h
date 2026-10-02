#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "generator/AmbiguitySearch.h"
#include "lr/CompressedParseTable.h"

namespace zbik {

struct CorpusLabelOptions {
    std::size_t maxLookahead = 6;
    std::size_t ambiguityMaxLength = 12;
    std::size_t ambiguityTreeLimit = 5000;
    std::uint32_t ambiguitySeed = 0;
    bool collectDetailedReport = false;
    bool stopAfterFirstConflict = true;
};

struct CorpusStageTiming {
    std::string stage;
    std::chrono::steady_clock::duration elapsed;
};

struct CorpusNonterminalSetStats {
    std::string nonterminal;
    std::size_t firstWords;
    std::size_t followWords;
};

struct CorpusLookaheadStats {
    std::size_t k;
    std::size_t firstWords;
    std::size_t followWords;
    std::size_t maxFirstWords;
    std::size_t maxFollowWords;
    std::vector<CorpusNonterminalSetStats> nonterminals;
};

struct CorpusAutomatonStats {
    std::string kind;
    std::size_t k;
    std::size_t states;
    std::size_t items;
    std::size_t transitions;
    std::size_t conflicts;
    std::size_t mergeConflicts;
    std::optional<TableStorageStats> storage;
};

struct CorpusGrammarReport {
    std::size_t index;
    std::vector<std::string> source;
    std::vector<std::string> labels;
    std::optional<AmbiguitySearchStatus> ambiguityStatus;
    std::optional<std::string> ambiguityDiagnostic;
    bool generatorTrap = false;
    std::vector<CorpusStageTiming> timings;
    std::vector<CorpusLookaheadStats> lookaheads;
    std::vector<CorpusAutomatonStats> automata;
    std::chrono::steady_clock::duration elapsed{};
    std::optional<std::string> error;
};

struct CorpusLabelResult {
    std::vector<std::string> lines;
    std::vector<CorpusGrammarReport> grammars;
    std::chrono::steady_clock::duration elapsed;

    [[nodiscard]] std::size_t unexpectedFailureCount() const noexcept;
};

using CorpusProgress =
        std::function<void(std::size_t grammarIndex, std::string_view stage)>;

[[nodiscard]] CorpusLabelResult labelGrammarCorpus(
        const std::vector<std::string> &lines,
        const CorpusLabelOptions &options = {},
        const CorpusProgress &progress = {});

[[nodiscard]] std::string formatCorpusReport(
        const CorpusLabelResult &result,
        const CorpusLabelOptions &options,
        std::string_view buildConfiguration);

} // namespace zbik
